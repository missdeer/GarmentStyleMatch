#include <algorithm>
#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>

#include "HttpDownloader.h"

namespace
{
    constexpr qsizetype kCopyBufferSize      = 1024 * 1024;
    constexpr int       kHttpOk              = 200;
    constexpr int       kHttpPartialContent  = 206;
    constexpr int       kHttpRedirectMinimum = 300;
    constexpr int       kHttpRedirectMaximum = 400;
    constexpr int       kMaximumRedirects    = 10;

    struct ContentRange
    {
        qint64 start = -1;
        qint64 end   = -1;
        qint64 total = -1;
    };

    ContentRange parseContentRange(const QByteArray &header)
    {
        static const QRegularExpression expression(QStringLiteral(R"(^bytes\s+(\d+)-(\d+)/(\d+)$)"));
        const QRegularExpressionMatch   match = expression.match(QString::fromLatin1(header).trimmed());
        if (!match.hasMatch())
        {
            return {};
        }
        return {match.captured(1).toLongLong(), match.captured(2).toLongLong(), match.captured(3).toLongLong()};
    }

    QNetworkRequest makeRequest(const QUrl &url)
    {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GarmentStyleMatch/%1").arg(QCoreApplication::applicationVersion()));
        return request;
    }

    bool isRedirectStatus(int status)
    {
        return status >= kHttpRedirectMinimum && status < kHttpRedirectMaximum;
    }

    bool resolveRedirect(QNetworkReply *reply, QUrl *target, QString *error)
    {
        const QUrl location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (location.isEmpty())
        {
            *error = QStringLiteral("服务器返回重定向，但未提供目标地址");
            return false;
        }
        *target = reply->url().resolved(location);
        if (!target->isValid() || (target->scheme() != QLatin1String("http") && target->scheme() != QLatin1String("https")))
        {
            *error = QStringLiteral("服务器返回了无效的重定向地址");
            return false;
        }
        if (reply->url().scheme() == QLatin1String("https") && target->scheme() != QLatin1String("https"))
        {
            *error = QStringLiteral("拒绝从 HTTPS 重定向到不安全地址");
            return false;
        }
        return true;
    }

    bool writeMetadata(const QString &directoryPath, const QUrl &url, qint64 totalSize, int segmentCount, bool rangeSupported)
    {
        QJsonObject metadata;
        metadata.insert(QStringLiteral("url"), url.toString(QUrl::FullyEncoded));
        metadata.insert(QStringLiteral("size"), QString::number(totalSize));
        metadata.insert(QStringLiteral("segments"), segmentCount);
        metadata.insert(QStringLiteral("rangeSupported"), rangeSupported);

        QSaveFile file(QDir(directoryPath).absoluteFilePath(QStringLiteral("download.json")));
        return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) >= 0 && file.commit();
    }

    bool metadataMatches(const QString &directoryPath, const QUrl &url, qint64 totalSize, int segmentCount, bool rangeSupported)
    {
        QFile file(QDir(directoryPath).absoluteFilePath(QStringLiteral("download.json")));
        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }
        const QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        return metadata.value(QStringLiteral("url")).toString() == url.toString(QUrl::FullyEncoded) &&
               metadata.value(QStringLiteral("size")).toString() == QString::number(totalSize) &&
               metadata.value(QStringLiteral("segments")).toInt() == segmentCount &&
               metadata.value(QStringLiteral("rangeSupported")).toBool() == rangeSupported;
    }
} // namespace

struct HttpDownloader::Segment
{
    int                    index             = 0;
    qint64                 start             = 0;
    qint64                 end               = -1;
    qint64                 requestStart      = 0;
    qint64                 expectedLength    = -1;
    bool                   rangeSupported    = false;
    bool                   responseValidated = false;
    int                    redirectCount     = 0;
    QUrl                   url;
    std::unique_ptr<QFile> file;
    QNetworkReply         *reply = nullptr;
};

HttpDownloader::HttpDownloader(QObject *parent) : QObject(parent), m_networkManager(new QNetworkAccessManager(this)) {}

HttpDownloader::~HttpDownloader()
{
    stopNetworkOperations();
}

void HttpDownloader::download(const QUrl &url, const QString &targetPath, int segmentCount)
{
    if (m_downloading)
    {
        return;
    }
    if (!url.isValid() || targetPath.isEmpty() || segmentCount <= 0)
    {
        emit failed(QStringLiteral("下载参数无效"));
        return;
    }
    if (!QDir().mkpath(QFileInfo(targetPath).absolutePath()))
    {
        emit failed(QStringLiteral("无法创建下载目录"));
        return;
    }

    stopNetworkOperations();
    m_url                 = url;
    m_resolvedUrl         = url;
    m_targetPath          = targetPath;
    m_totalSize           = -1;
    m_receivedBytes       = 0;
    m_requestedSegments   = segmentCount;
    m_completedSegments   = 0;
    m_probeRedirectCount  = 0;
    m_probeHandled        = false;
    m_probeRangeSupported = false;
    m_probeError.clear();
    m_downloading = true;
    startProbe();
}

void HttpDownloader::cancel()
{
    if (!m_downloading)
    {
        return;
    }
    m_downloading = false;
    stopNetworkOperations();
    emit canceled();
}

void HttpDownloader::startProbe()
{
    QNetworkRequest request = makeRequest(m_resolvedUrl);
    auto           *reply   = m_networkManager->head(request);
    m_probeReply            = reply;
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply] { handleProbeHeaders(reply); });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleProbeFinished(reply); });
}

void HttpDownloader::handleProbeHeaders(QNetworkReply *reply)
{
    if (!m_downloading || reply != m_probeReply || m_probeHandled)
    {
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0)
    {
        return;
    }
    if (isRedirectStatus(status))
    {
        return;
    }

    m_probeHandled = true;
    if (status == kHttpOk)
    {
        m_totalSize = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (m_totalSize <= 0)
        {
            m_probeError = QStringLiteral("服务器 HEAD 响应未返回有效的 Content-Length");
        }
        m_probeRangeSupported =
            reply->rawHeader(QByteArrayLiteral("Accept-Ranges")).trimmed().compare(QByteArrayLiteral("bytes"), Qt::CaseInsensitive) == 0;
    }
    else
    {
        m_probeError = QStringLiteral("服务器返回 HTTP %1").arg(status);
    }
}

void HttpDownloader::handleProbeFinished(QNetworkReply *reply)
{
    if (reply != m_probeReply)
    {
        return;
    }
    if (!m_probeHandled)
    {
        handleProbeHeaders(reply);
    }
    const int  status   = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool redirect = isRedirectStatus(status);
    QUrl       redirectUrl;
    QString    redirectError;
    const bool redirectValid                       = !redirect || resolveRedirect(reply, &redirectUrl, &redirectError);
    m_probeReply                                   = nullptr;
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString                     errorText    = reply->errorString();
    reply->deleteLater();

    if (!m_downloading)
    {
        return;
    }
    if (redirect)
    {
        if (networkError != QNetworkReply::NoError)
        {
            failDownload(errorText);
            return;
        }
        if (!redirectValid)
        {
            failDownload(redirectError);
            return;
        }
        ++m_probeRedirectCount;
        if (m_probeRedirectCount > kMaximumRedirects)
        {
            failDownload(QStringLiteral("服务器重定向次数过多"));
            return;
        }
        m_resolvedUrl = std::move(redirectUrl);
        startProbe();
        return;
    }
    if (!m_probeError.isEmpty())
    {
        failDownload(m_probeError);
        return;
    }
    if (!m_probeHandled || networkError != QNetworkReply::NoError)
    {
        failDownload(errorText);
        return;
    }
    startSegments(m_probeRangeSupported);
}

void HttpDownloader::startSegments(bool rangeSupported)
{
    const int     segmentCount  = rangeSupported ? static_cast<int>(std::min<qint64>(m_requestedSegments, m_totalSize)) : 1;
    const QString directoryPath = temporaryDirectoryPath();
    QDir          directory(directoryPath);
    if (directory.exists() && !metadataMatches(directoryPath, m_url, m_totalSize, segmentCount, rangeSupported) && !directory.removeRecursively())
    {
        failDownload(QStringLiteral("无法清理不兼容的下载临时文件"));
        return;
    }
    if (!QDir().mkpath(directoryPath) || !writeMetadata(directoryPath, m_url, m_totalSize, segmentCount, rangeSupported))
    {
        failDownload(QStringLiteral("无法创建下载临时文件"));
        return;
    }

    m_segments.clear();
    m_segments.reserve(static_cast<std::size_t>(segmentCount));
    m_receivedBytes         = 0;
    m_completedSegments     = 0;
    const qint64 baseLength = m_totalSize > 0 ? m_totalSize / segmentCount : -1;
    const qint64 remainder  = m_totalSize > 0 ? m_totalSize % segmentCount : 0;
    qint64       start      = 0;

    for (int index = 0; index < segmentCount; ++index)
    {
        const qint64 expectedLength = m_totalSize > 0 ? baseLength + (index < remainder ? 1 : 0) : -1;
        if (!prepareSegment(index, start, expectedLength, rangeSupported, directory))
        {
            return;
        }
        start += std::max<qint64>(expectedLength, 0);
    }

    emit progress(m_receivedBytes, m_totalSize);
    if (m_completedSegments == segmentCount)
    {
        assembleTarget();
        return;
    }
    for (const auto &segment : m_segments)
    {
        if (segment->expectedLength < 0 || segment->requestStart <= segment->end)
        {
            startSegmentRequest(segment.get());
        }
    }
}

bool HttpDownloader::prepareSegment(int index, qint64 start, qint64 expectedLength, bool rangeSupported, const QDir &directory)
{
    auto segment            = std::make_unique<Segment>();
    segment->index          = index;
    segment->start          = start;
    segment->expectedLength = expectedLength;
    segment->end            = expectedLength > 0 ? start + expectedLength - 1 : -1;
    segment->rangeSupported = rangeSupported;
    segment->url            = m_resolvedUrl;
    segment->file           = std::make_unique<QFile>(directory.absoluteFilePath(QStringLiteral("part-%1").arg(index)));

    qint64     existingSize = segment->file->exists() ? segment->file->size() : 0;
    const bool resetExisting =
        (expectedLength >= 0 && existingSize > expectedLength) || (!rangeSupported && existingSize > 0 && existingSize != expectedLength);
    if (resetExisting)
    {
        if (segment->file->exists() && !segment->file->remove())
        {
            failDownload(QStringLiteral("无法重置下载分片 %1").arg(index + 1));
            return false;
        }
        existingSize = 0;
    }
    segment->requestStart = start + existingSize;
    m_receivedBytes += existingSize;
    if (expectedLength >= 0 && existingSize == expectedLength)
    {
        ++m_completedSegments;
    }
    else if (!segment->file->open(QIODevice::WriteOnly | QIODevice::Append))
    {
        failDownload(QStringLiteral("无法写入下载分片 %1：%2").arg(index + 1).arg(segment->file->errorString()));
        return false;
    }
    m_segments.push_back(std::move(segment));
    return true;
}

void HttpDownloader::startSegmentRequest(Segment *segment)
{
    QNetworkRequest request = makeRequest(segment->url);
    if (segment->rangeSupported)
    {
        request.setRawHeader(QByteArrayLiteral("Range"),
                             QByteArrayLiteral("bytes=") + QByteArray::number(segment->requestStart) + '-' + QByteArray::number(segment->end));
    }
    segment->reply = m_networkManager->get(request);
    connect(segment->reply, &QNetworkReply::metaDataChanged, this, [this, segment] { handleSegmentHeaders(segment); });
    connect(segment->reply, &QNetworkReply::readyRead, this, [this, segment] { writeAvailableData(segment); });
    connect(segment->reply, &QNetworkReply::finished, this, [this, segment] { handleSegmentFinished(segment); });
}

void HttpDownloader::handleSegmentHeaders(Segment *segment)
{
    if (!m_downloading || segment->responseValidated || !segment->reply)
    {
        return;
    }
    const int status = segment->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0)
    {
        return;
    }
    if (isRedirectStatus(status))
    {
        return;
    }
    if (segment->rangeSupported)
    {
        const ContentRange range = parseContentRange(segment->reply->rawHeader(QByteArrayLiteral("Content-Range")));
        if (status != kHttpPartialContent || range.start != segment->requestStart || range.end != segment->end || range.total != m_totalSize)
        {
            failDownload(QStringLiteral("服务器未按请求返回分片 %1").arg(segment->index + 1));
            return;
        }
    }
    else if (status != kHttpOk)
    {
        failDownload(QStringLiteral("服务器返回 HTTP %1").arg(status));
        return;
    }
    segment->responseValidated = true;
}

void HttpDownloader::writeAvailableData(Segment *segment)
{
    if (!m_downloading || !segment->reply)
    {
        return;
    }
    handleSegmentHeaders(segment);
    if (!m_downloading || !segment->responseValidated)
    {
        return;
    }
    const QByteArray data = segment->reply->readAll();
    if (data.isEmpty())
    {
        return;
    }
    const qint64 written = segment->file->write(data);
    if (written != data.size())
    {
        failDownload(QStringLiteral("写入下载分片 %1 失败：%2").arg(segment->index + 1).arg(segment->file->errorString()));
        return;
    }
    m_receivedBytes += written;
    emit progress(m_receivedBytes, m_totalSize);
}

void HttpDownloader::handleSegmentFinished(Segment *segment)
{
    if (!m_downloading || !segment->reply)
    {
        return;
    }
    const int status = segment->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (isRedirectStatus(status))
    {
        auto *reply    = segment->reply;
        segment->reply = nullptr;
        QUrl                              redirectUrl;
        QString                           redirectError;
        const bool                        redirectValid = resolveRedirect(reply, &redirectUrl, &redirectError);
        const QNetworkReply::NetworkError networkError  = reply->error();
        const QString                     errorText     = reply->errorString();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError)
        {
            failDownload(errorText);
            return;
        }
        if (!redirectValid)
        {
            failDownload(redirectError);
            return;
        }
        ++segment->redirectCount;
        if (segment->redirectCount > kMaximumRedirects)
        {
            failDownload(QStringLiteral("服务器重定向次数过多"));
            return;
        }
        segment->url               = std::move(redirectUrl);
        segment->responseValidated = false;
        startSegmentRequest(segment);
        return;
    }
    handleSegmentHeaders(segment);
    writeAvailableData(segment);
    if (!m_downloading || !segment->reply)
    {
        return;
    }

    auto *reply    = segment->reply;
    segment->reply = nullptr;
    segment->file->close();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString                     errorText    = reply->errorString();
    reply->deleteLater();
    if (networkError != QNetworkReply::NoError)
    {
        failDownload(errorText);
        return;
    }
    if (segment->expectedLength >= 0 && segment->file->size() != segment->expectedLength)
    {
        failDownload(QStringLiteral("下载分片 %1 大小不完整").arg(segment->index + 1));
        return;
    }

    ++m_completedSegments;
    if (m_completedSegments == static_cast<int>(m_segments.size()))
    {
        assembleTarget();
    }
}

void HttpDownloader::assembleTarget()
{
    QSaveFile target(m_targetPath);
    if (!target.open(QIODevice::WriteOnly))
    {
        failDownload(QStringLiteral("无法创建目标文件：%1").arg(target.errorString()));
        return;
    }

    QByteArray buffer(kCopyBufferSize, Qt::Uninitialized);
    for (const auto &segment : m_segments)
    {
        if (segment->file->isOpen())
        {
            segment->file->close();
        }
        if (!segment->file->open(QIODevice::ReadOnly))
        {
            failDownload(QStringLiteral("无法读取下载分片 %1：%2").arg(segment->index + 1).arg(segment->file->errorString()));
            return;
        }
        while (!segment->file->atEnd())
        {
            const qint64 read = segment->file->read(buffer.data(), buffer.size());
            if (read <= 0 || target.write(buffer.constData(), read) != read)
            {
                failDownload(QStringLiteral("合并下载分片失败"));
                return;
            }
        }
        segment->file->close();
    }
    if (!target.commit())
    {
        failDownload(QStringLiteral("无法保存目标文件：%1").arg(target.errorString()));
        return;
    }
    QDir(temporaryDirectoryPath()).removeRecursively();

    m_downloading = false;
    m_segments.clear();
    emit progress(m_totalSize, m_totalSize);
    emit finished();
}

void HttpDownloader::stopNetworkOperations()
{
    if (m_probeReply)
    {
        disconnect(m_probeReply, nullptr, this, nullptr);
        m_probeReply->abort();
        m_probeReply->deleteLater();
        m_probeReply = nullptr;
    }
    for (const auto &segment : m_segments)
    {
        if (segment->reply)
        {
            disconnect(segment->reply, nullptr, this, nullptr);
            segment->reply->abort();
            segment->reply->deleteLater();
            segment->reply = nullptr;
        }
        if (segment->file && segment->file->isOpen())
        {
            segment->file->close();
        }
    }
    m_segments.clear();
}

void HttpDownloader::failDownload(const QString &error)
{
    if (!m_downloading)
    {
        return;
    }
    m_downloading = false;
    stopNetworkOperations();
    emit failed(error);
}

QString HttpDownloader::temporaryDirectoryPath() const
{
    return m_targetPath + QStringLiteral(".download");
}
