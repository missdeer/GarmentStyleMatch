#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "HttpDownloader.h"

namespace
{
    constexpr int       kTimeoutMs            = 5000;
    constexpr int       kDrainSliceMs         = 10;
    constexpr int       kSlowChunkDelayMs     = 5;
    constexpr qsizetype kSlowChunkSize        = 1024;
    constexpr qsizetype kTestPayloadSize      = 512 * 1024;
    constexpr qsizetype kPayloadPatternPeriod = 251;

    bool check(bool condition, const QString &message)
    {
        if (condition)
        {
            return true;
        }
        std::cerr << message.toStdString() << '\n';
        return false;
    }

    class RangeServer final : public QObject
    {
    public:
        explicit RangeServer(QByteArray payload, QObject *parent = nullptr) : QObject(parent), m_payload(std::move(payload))
        {
            connect(&m_server, &QTcpServer::newConnection, this, [this] {
                while (QTcpSocket *socket = m_server.nextPendingConnection())
                {
                    m_activeSockets.insert(socket);
                    connect(socket, &QObject::destroyed, this, [this, socket] { m_activeSockets.remove(socket); });
                    auto request = std::make_shared<QByteArray>();
                    connect(socket, &QTcpSocket::readyRead, this, [this, socket, request] {
                        request->append(socket->readAll());
                        if (!request->contains("\r\n\r\n"))
                        {
                            return;
                        }
                        disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
                        respond(socket, *request);
                    });
                    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                }
            });
        }

        bool listen()
        {
            return m_server.listen(QHostAddress::LocalHost);
        }

        [[nodiscard]] QUrl url() const
        {
            return {QStringLiteral("http://127.0.0.1:%1/model.bin").arg(m_server.serverPort())};
        }

        void setSlow(bool slow)
        {
            m_slow = slow;
        }

        void setRangeSupported(bool supported)
        {
            m_rangeSupported = supported;
        }

        void setRedirectEnabled(bool enabled)
        {
            m_redirectEnabled = enabled;
        }

        void clearRanges()
        {
            m_ranges.clear();
        }

        bool drainConnections(int timeoutMs)
        {
            QDeadlineTimer deadline(timeoutMs);
            while (!m_activeSockets.isEmpty() && !deadline.hasExpired())
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, kDrainSliceMs);
            }
            return m_activeSockets.isEmpty();
        }

        [[nodiscard]] const std::vector<std::pair<qint64, qint64>> &ranges() const
        {
            return m_ranges;
        }

        [[nodiscard]] int redirectCount() const
        {
            return m_redirectCount;
        }

        [[nodiscard]] int headRequestCount() const
        {
            return m_headRequestCount;
        }

    private:
        void respond(QTcpSocket *socket, const QByteArray &request)
        {
            const bool headRequest = request.startsWith("HEAD ");
            if (headRequest)
            {
                ++m_headRequestCount;
            }
            if (m_redirectEnabled && (request.startsWith("GET /model.bin ") || request.startsWith("HEAD /model.bin ")))
            {
                ++m_redirectCount;
                socket->write(
                    QByteArrayLiteral("HTTP/1.1 302 Found\r\nLocation: /redirected-model.bin\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
                socket->disconnectFromHost();
                return;
            }
            if (headRequest)
            {
                QByteArray headers = QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Length: ") + QByteArray::number(m_payload.size());
                if (m_rangeSupported)
                {
                    headers += QByteArrayLiteral("\r\nAccept-Ranges: bytes");
                }
                headers += QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
                socket->write(headers);
                socket->disconnectFromHost();
                return;
            }

            static const QRegularExpression rangeExpression(QStringLiteral(R"(Range:\s*bytes=(\d+)-(\d*))"),
                                                            QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch   match = rangeExpression.match(QString::fromLatin1(request));
            qint64                          start = 0;
            qint64                          end   = m_payload.size() - 1;
            if (m_rangeSupported && match.hasMatch())
            {
                start = match.captured(1).toLongLong();
                if (!match.captured(2).isEmpty())
                {
                    end = match.captured(2).toLongLong();
                }
                m_ranges.emplace_back(start, end);
            }
            const QByteArray body = m_payload.mid(start, end - start + 1);
            QByteArray headers    = (m_rangeSupported && match.hasMatch() ? QByteArrayLiteral("HTTP/1.1 206 Partial Content\r\n")
                                                                          : QByteArrayLiteral("HTTP/1.1 200 OK\r\n")) +
                                    QByteArrayLiteral("Content-Type: application/octet-stream\r\nContent-Length: ") + QByteArray::number(body.size());
            if (m_rangeSupported && match.hasMatch())
            {
                headers += QByteArrayLiteral("\r\nContent-Range: bytes ") + QByteArray::number(start) + '-' + QByteArray::number(end) + '/' +
                           QByteArray::number(m_payload.size());
            }
            headers += QByteArrayLiteral("\r\nConnection: close\r\n\r\n");
            socket->write(headers);
            if (!m_slow || (start == 0 && end == 0))
            {
                socket->write(body);
                socket->disconnectFromHost();
                return;
            }

            auto                 offset = std::make_shared<qsizetype>(0);
            auto                 pump   = std::make_shared<std::function<void()>>();
            QPointer<QTcpSocket> guardedSocket(socket);
            *pump = [guardedSocket, body, offset, pump] {
                if (!guardedSocket || guardedSocket->state() == QAbstractSocket::UnconnectedState)
                {
                    return;
                }
                const qsizetype remaining = body.size() - *offset;
                const qsizetype size      = std::min(kSlowChunkSize, remaining);
                if (size > 0)
                {
                    guardedSocket->write(body.mid(*offset, size));
                    *offset += size;
                }
                if (*offset == body.size())
                {
                    guardedSocket->disconnectFromHost();
                    return;
                }
                QTimer::singleShot(kSlowChunkDelayMs, guardedSocket, *pump);
            };
            (*pump)();
        }

        QTcpServer                             m_server;
        QByteArray                             m_payload;
        std::vector<std::pair<qint64, qint64>> m_ranges;
        QSet<QTcpSocket *>                     m_activeSockets;
        bool                                   m_slow             = false;
        bool                                   m_rangeSupported   = true;
        bool                                   m_redirectEnabled  = false;
        int                                    m_redirectCount    = 0;
        int                                    m_headRequestCount = 0;
    };
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QByteArray       payload(kTestPayloadSize, Qt::Uninitialized);
    qsizetype        patternIndex = 0;
    for (char &byte : payload)
    {
        byte = static_cast<char>(patternIndex % kPayloadPatternPeriod);
        ++patternIndex;
    }

    RangeServer   server(payload);
    QTemporaryDir temporary;
    if (!check(server.listen() && temporary.isValid(), QStringLiteral("无法启动本地分片下载测试")))
    {
        return 1;
    }

    const QString  targetPath = temporary.filePath(QStringLiteral("model.bin"));
    HttpDownloader downloader;
    QEventLoop     cancelLoop;
    bool           canceled = false;
    QString        cancelFailure;
    QObject::connect(&downloader, &HttpDownloader::canceled, &cancelLoop, [&] {
        canceled = true;
        cancelLoop.quit();
    });
    QObject::connect(&downloader, &HttpDownloader::failed, &cancelLoop, [&](const QString &error) {
        cancelFailure = error;
        cancelLoop.quit();
    });
    const QMetaObject::Connection cancelOnProgress =
        QObject::connect(&downloader, &HttpDownloader::progress, &cancelLoop, [&](qint64 received, qint64) {
            if (received > 0 && downloader.isDownloading())
            {
                downloader.cancel();
            }
        });
    server.setSlow(true);
    server.setRedirectEnabled(true);
    downloader.download(server.url(), targetPath);
    QTimer::singleShot(kTimeoutMs, &cancelLoop, &QEventLoop::quit);
    cancelLoop.exec();
    QObject::disconnect(cancelOnProgress);
    if (!check(canceled && QFileInfo::exists(targetPath + QStringLiteral(".download")),
               QStringLiteral("取消下载必须保留可续传的临时分片，错误：%1").arg(cancelFailure)))
    {
        return 1;
    }

    server.setSlow(false);
    if (!check(server.drainConnections(kTimeoutMs), QStringLiteral("首次下载的服务器分片连接必须在续传前排空")))
    {
        return 1;
    }
    server.clearRanges();
    QEventLoop finishLoop;
    bool       finished = false;
    QString    failure;
    QObject::connect(&downloader, &HttpDownloader::finished, &finishLoop, [&] {
        finished = true;
        finishLoop.quit();
    });
    QObject::connect(&downloader, &HttpDownloader::failed, &finishLoop, [&](const QString &error) {
        failure = error;
        finishLoop.quit();
    });
    downloader.download(server.url(), targetPath);
    QTimer::singleShot(kTimeoutMs, &finishLoop, &QEventLoop::quit);
    finishLoop.exec();

    QFile target(targetPath);
    if (!check(finished && failure.isEmpty() && target.open(QIODevice::ReadOnly) && target.readAll() == payload,
               QStringLiteral("续传完成后目标文件必须与服务器内容一致，错误：%1").arg(failure)))
    {
        return 1;
    }

    const auto &ranges           = server.ranges();
    const auto  dataRequestCount = std::ranges::count_if(ranges, [](const auto &range) { return range.second > 0; });
    if (!check(dataRequestCount == HttpDownloader::DefaultSegmentCount, QStringLiteral("默认下载必须使用 5 个并发分片")))
    {
        return 1;
    }

    const qint64        baseLength = payload.size() / HttpDownloader::DefaultSegmentCount;
    const qint64        remainder  = payload.size() % HttpDownloader::DefaultSegmentCount;
    std::vector<qint64> nominalStarts;
    qint64              start = 0;
    for (int index = 0; index < HttpDownloader::DefaultSegmentCount; ++index)
    {
        nominalStarts.push_back(start);
        start += baseLength + (index < remainder ? 1 : 0);
    }
    const bool resumed = std::ranges::any_of(
        ranges, [&](const auto &range) { return range.second > 0 && std::ranges::find(nominalStarts, range.first) == nominalStarts.cend(); });
    if (!check(resumed, QStringLiteral("再次下载必须从已保存的分片偏移继续，而不是全部从头开始")))
    {
        return 1;
    }
    if (!check(server.redirectCount() > 0, QStringLiteral("下载必须自动跟随服务器的 HTTP 302 重定向")))
    {
        return 1;
    }
    if (!check(server.headRequestCount() > 0, QStringLiteral("下载前必须通过 HEAD 请求获取远端文件大小")))
    {
        return 1;
    }
    if (!check(!QFileInfo::exists(targetPath + QStringLiteral(".download")), QStringLiteral("下载成功后必须清理临时分片目录")))
    {
        return 1;
    }

    server.setRangeSupported(false);
    const QString fallbackTargetPath = temporary.filePath(QStringLiteral("fallback-model.bin"));
    QEventLoop    fallbackLoop;
    bool          fallbackFinished = false;
    QString       fallbackFailure;
    QObject::connect(&downloader, &HttpDownloader::finished, &fallbackLoop, [&] {
        fallbackFinished = true;
        fallbackLoop.quit();
    });
    QObject::connect(&downloader, &HttpDownloader::failed, &fallbackLoop, [&](const QString &error) {
        fallbackFailure = error;
        fallbackLoop.quit();
    });
    downloader.download(server.url(), fallbackTargetPath);
    QTimer::singleShot(kTimeoutMs, &fallbackLoop, &QEventLoop::quit);
    fallbackLoop.exec();

    QFile fallbackTarget(fallbackTargetPath);
    if (!check(fallbackFinished && fallbackFailure.isEmpty() && fallbackTarget.open(QIODevice::ReadOnly) && fallbackTarget.readAll() == payload,
               QStringLiteral("服务器不支持 Range 时必须回退到单流下载，错误：%1").arg(fallbackFailure)))
    {
        return 1;
    }
    return 0;
}
