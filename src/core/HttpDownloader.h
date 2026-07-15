#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QDir;

class HttpDownloader final : public QObject
{
    Q_OBJECT

public:
    static constexpr int DefaultSegmentCount = 5;

    explicit HttpDownloader(QObject *parent = nullptr);
    ~HttpDownloader() override;

    [[nodiscard]] bool isDownloading() const
    {
        return m_downloading;
    }

    void download(const QUrl &url, const QString &targetPath, int segmentCount = DefaultSegmentCount);

public slots:
    void cancel();

signals:
    void progress(qint64 received, qint64 total);
    void finished();
    void failed(const QString &error);
    void canceled();

private:
    Q_DISABLE_COPY_MOVE(HttpDownloader)

    struct Segment;

    void                  startProbe();
    void                  handleProbeHeaders(QNetworkReply *reply);
    void                  handleProbeFinished(QNetworkReply *reply);
    void                  startSegments(bool rangeSupported);
    bool                  prepareSegment(int index, qint64 start, qint64 expectedLength, bool rangeSupported, const QDir &directory);
    void                  startSegmentRequest(Segment *segment);
    void                  handleSegmentHeaders(Segment *segment);
    void                  writeAvailableData(Segment *segment);
    void                  handleSegmentFinished(Segment *segment);
    void                  assembleTarget();
    void                  stopNetworkOperations();
    void                  failDownload(const QString &error);
    [[nodiscard]] QString temporaryDirectoryPath() const;

    QNetworkAccessManager                *m_networkManager = nullptr;
    QNetworkReply                        *m_probeReply     = nullptr;
    std::vector<std::unique_ptr<Segment>> m_segments;
    QUrl                                  m_url;
    QUrl                                  m_resolvedUrl;
    QString                               m_targetPath;
    qint64                                m_totalSize           = -1;
    qint64                                m_receivedBytes       = 0;
    int                                   m_requestedSegments   = DefaultSegmentCount;
    int                                   m_completedSegments   = 0;
    int                                   m_probeRedirectCount  = 0;
    bool                                  m_downloading         = false;
    bool                                  m_probeHandled        = false;
    bool                                  m_probeRangeSupported = false;
    QString                               m_probeError;
};
