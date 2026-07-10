#include "MatchController.h"
#include "CandidateListModel.h"
#include "GalleryListModel.h"
#include "PhotoListModel.h"
#include "PptPageListModel.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#include <QAxObject>
#endif

MatchController::MatchController(QObject *parent)
    : QObject(parent)
{
}

void MatchController::setCandidateModel(CandidateListModel *m)
{
    m_candidateModel = m;
}

void MatchController::setGalleryModel(GalleryListModel *m)
{
    m_galleryModel = m;
}

void MatchController::setPhotoModel(PhotoListModel *m)
{
    m_photoModel = m;
}

void MatchController::setPptPageModel(PptPageListModel *m)
{
    m_pptPageModel = m;
}

QString MatchController::title() const
{
    return QString::fromUtf8("Eidos\xE6\x9C\x8D\xE8\xA3\x85\xE6\xAC\xBE\xE5\xBC\x8F\xE5\x8C\xB9\xE9\x85\x8D\xE5\xB7\xA5\xE4\xBD\x9C\xE5\x8F\xB0");
}

QString MatchController::subtitle() const
{
    // "联网AI识别版 <date>"
    const QString badge = QString::fromUtf8(
        "\xE8\x81\x94\xE7\xBD\x91""AI"
        "\xE8\xAF\x86\xE5\x88\xAB\xE7\x89\x88");
    return QStringLiteral("%1 %2").arg(badge, QDate::currentDate().toString(Qt::ISODate));
}

int MatchController::currentImageCount() const
{
    if (m_previewSource == PreviewPhoto)
        return m_photoModel && m_currentPhotoIndex >= 0 ? 1 : 0;
    if (!m_candidateModel)
        return 0;
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it ? it->candidateCount : 0;
}

QString MatchController::currentImagePath() const
{
    if (m_previewSource == PreviewPhoto) {
        if (!m_photoModel) return {};
        const auto *p = m_photoModel->at(m_currentPhotoIndex);
        return p ? p->imagePath : QString();
    }
    if (!m_candidateModel)
        return {};
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it ? it->imagePath : QString();
}

QString MatchController::currentStyleId() const
{
    if (m_previewSource == PreviewPhoto) {
        if (!m_photoModel) return {};
        const auto *p = m_photoModel->at(m_currentPhotoIndex);
        return p ? p->fileName : QString();
    }
    if (!m_candidateModel)
        return {};
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it ? it->styleId : QString();
}

void MatchController::setCurrentIndex(int idx)
{
    const bool sourceChanged = (m_previewSource != PreviewOutput);
    if (idx == m_currentIndex && !sourceChanged)
        return;
    m_currentIndex = idx;
    m_previewSource = PreviewOutput;
    m_currentImagePage = 0;
    emitCurrentChanged();
}

void MatchController::setCurrentPhotoIndex(int idx)
{
    const bool sourceChanged = (m_previewSource != PreviewPhoto);
    if (idx == m_currentPhotoIndex && !sourceChanged)
        return;
    m_currentPhotoIndex = idx;
    m_previewSource = PreviewPhoto;
    m_currentImagePage = 0;
    emit currentPhotoIndexChanged();
    emitCurrentChanged();
    emit logMessage(QStringLiteral("selectPhoto row=%1").arg(idx));
}

void MatchController::setCategoryFilter(const QString &v)
{
    if (v == m_categoryFilter)
        return;
    m_categoryFilter = v;
    emit categoryFilterChanged();
}

void MatchController::setSearchText(const QString &v)
{
    if (v == m_searchText)
        return;
    m_searchText = v;
    emit searchTextChanged();
}

void MatchController::setPhotoDir(const QString &v)
{
    if (v == m_photoDir)
        return;
    m_photoDir = v;
    emit photoDirChanged();
    emit logMessage(QStringLiteral("photoDir=%1").arg(v));
    scanPhotoDir();
}

void MatchController::setOutputDir(const QString &v)
{
    if (v == m_outputDir)
        return;
    m_outputDir = v;
    emit outputDirChanged();
    emit logMessage(QStringLiteral("outputDir=%1").arg(v));
    scanOutputDir();
}

void MatchController::setPptPath(const QString &v)
{
    if (v == m_pptPath)
        return;
    m_pptPath = v;
    emit pptPathChanged();
    emit logMessage(QStringLiteral("pptPath=%1").arg(v));
    loadPptPreviewsFromCache();
}

QString MatchController::pptCacheDir(const QString &pptFilePath) const
{
    const QFileInfo fi(pptFilePath);
    if (!fi.exists() || !fi.isFile())
        return {};
    const QString key = fi.fileName() + QLatin1Char(':') + QString::number(fi.size());
    const QByteArray hash = QCryptographicHash::hash(
        key.toUtf8(), QCryptographicHash::Sha1);
    const QString subDir = QString::fromLatin1(hash.toHex().left(16));
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation);
    return base + QStringLiteral("/ppt_slides/") + subDir;
}

bool MatchController::loadPptPreviewsFromCache()
{
    if (!m_pptPageModel)
        return false;
    m_pptPageModel->clear();
    if (m_pptPath.isEmpty())
        return false;

    const QString dir = pptCacheDir(m_pptPath);
    if (dir.isEmpty())
        return false;
    QDir d(dir);
    if (!d.exists())
        return false;
    const auto files = d.entryInfoList({QStringLiteral("slide_*.png")},
                                       QDir::Files | QDir::NoDotAndDotDot,
                                       QDir::Name);
    if (files.isEmpty())
        return false;

    for (int i = 0; i < files.size(); ++i) {
        PptPageItem p;
        p.pageIndex = i + 1;
        p.imagePath = files.at(i).absoluteFilePath();
        m_pptPageModel->appendItem(p);
    }
    emit logMessage(QStringLiteral("从缓存加载 %1 页预览: %2").arg(files.size()).arg(dir));
    return true;
}

void MatchController::scanPhotoDir()
{
    if (!m_photoModel) {
        emit logMessage(QStringLiteral("scanPhotoDir: no photoModel"));
        return;
    }

    QVector<PhotoItem> items;
    if (!m_photoDir.isEmpty()) {
        static const QStringList imgFilter = {
            QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
        };
        QDirIterator it(m_photoDir, imgFilter,
                        QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            PhotoItem p;
            p.fileName  = fi.fileName();
            p.imagePath = fi.absoluteFilePath();
            items.push_back(p);
        }
        std::sort(items.begin(), items.end(),
                  [](const PhotoItem &a, const PhotoItem &b) {
                      return a.imagePath < b.imagePath;
                  });
    }
    m_photoModel->setItems(std::move(items));
    setCurrentPhotoIndex(m_photoModel->rowCount() > 0 ? 0 : -1);
    emit logMessage(QStringLiteral("scanPhotoDir=%1 (%2 files)")
                    .arg(m_photoDir).arg(m_photoModel->rowCount()));
}

void MatchController::scanOutputDir()
{
    if (!m_candidateModel)
        return;

    QVector<CandidateItem> items;
    if (!m_outputDir.isEmpty()) {
        static const QStringList imgFilter = {
            QStringLiteral("*.png"),  QStringLiteral("*.jpg"),
            QStringLiteral("*.jpeg"), QStringLiteral("*.bmp"),
            QStringLiteral("*.webp"),
        };
        QDir dir(m_outputDir);
        const auto entries = dir.entryInfoList(imgFilter,
                                               QDir::Files | QDir::NoDotAndDotDot,
                                               QDir::Name);
        items.reserve(entries.size());
        for (const QFileInfo &fi : entries) {
            CandidateItem it;
            it.styleId        = fi.completeBaseName();
            it.imagePath      = fi.absoluteFilePath();
            it.candidateCount = 1;
            it.score          = 0.0;
            it.confirmed      = false;
            items.push_back(it);
        }
    }
    m_candidateModel->setItems(std::move(items));
    setCurrentIndex(m_candidateModel->rowCount() > 0 ? 0 : -1);
    emit logMessage(QStringLiteral("scanOutputDir=%1 (%2 files)")
                    .arg(m_outputDir).arg(m_candidateModel->rowCount()));
}

void MatchController::setBusy(bool on)
{
    if (m_busy == on)
        return;
    m_busy = on;
    emit busyChanged();
}

void MatchController::reloadPpt()
{
    emit logMessage(QStringLiteral("reloadPpt=%1").arg(m_pptPath));
    if (!m_pptPageModel)
        return;

    setBusy(true);
    auto busyGuard = qScopeGuard([this] { setBusy(false); });

    m_pptPageModel->clear();

    if (m_pptPath.isEmpty())
        return;

    const QFileInfo fi(m_pptPath);
    if (!fi.exists() || !fi.isFile()) {
        emit logMessage(QStringLiteral("PPT 文件不存在: %1").arg(m_pptPath));
        return;
    }

#ifdef Q_OS_WIN
    const QString cacheDir = pptCacheDir(m_pptPath);
    if (cacheDir.isEmpty()) {
        emit logMessage(QStringLiteral("无法确定缓存目录"));
        return;
    }
    QDir dir(cacheDir);
    if (!dir.exists())
        QDir().mkpath(cacheDir);
    const auto stale = dir.entryInfoList({QStringLiteral("slide_*.png")},
                                         QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &f : stale)
        QFile::remove(f.absoluteFilePath());

    QAxObject ppApp(QStringLiteral("PowerPoint.Application"));
    if (ppApp.isNull()) {
        emit logMessage(QStringLiteral("PowerPoint COM 启动失败,请确认已安装 Microsoft PowerPoint"));
        return;
    }

    QAxObject *presentations = ppApp.querySubObject("Presentations");
    if (!presentations) {
        emit logMessage(QStringLiteral("获取 Presentations 集合失败"));
        ppApp.dynamicCall("Quit()");
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
    // Open(FileName, ReadOnly=msoTrue, Untitled=msoFalse, WithWindow=msoFalse)
    QAxObject *pres = presentations->querySubObject(
        "Open(const QString&,int,int,int)",
        nativePath, -1, 0, 0);
    if (!pres) {
        emit logMessage(QStringLiteral("PPT 打开失败: %1").arg(nativePath));
        delete presentations;
        ppApp.dynamicCall("Quit()");
        return;
    }

    QAxObject *slides = pres->querySubObject("Slides");
    const int count = slides ? slides->property("Count").toInt() : 0;
    emit logMessage(QStringLiteral("PPT 共 %1 页,开始导出预览...").arg(count));

    constexpr int kExportW = 800;
    constexpr int kExportH = 600;
    for (int i = 1; i <= count; ++i) {
        QAxObject *slide = slides->querySubObject("Item(int)", i);
        if (!slide)
            continue;
        const QString outPath = QDir::toNativeSeparators(
            cacheDir + QStringLiteral("/slide_%1.png").arg(i, 4, 10, QChar('0')));
        slide->dynamicCall("Export(const QString&,const QString&,int,int)",
                           outPath, QStringLiteral("PNG"), kExportW, kExportH);
        delete slide;

        const QFileInfo outFi(outPath);
        if (!outFi.exists() || outFi.size() <= 0) {
            emit logMessage(QStringLiteral("警告: slide %1 导出失败或文件为空: %2")
                                .arg(i).arg(outPath));
            QCoreApplication::processEvents();
            continue;
        }

        PptPageItem p;
        p.pageIndex = i;
        p.imagePath = QDir::fromNativeSeparators(outPath);
        m_pptPageModel->appendItem(p);

        emit logMessage(QStringLiteral("导出 slide %1/%2 (%3 KB, rowCount=%4)")
                            .arg(i).arg(count).arg(outFi.size() / 1024)
                            .arg(m_pptPageModel->rowCount()));
        QCoreApplication::processEvents();
    }

    delete slides;
    pres->dynamicCall("Close()");
    delete pres;
    delete presentations;
    ppApp.dynamicCall("Quit()");

    emit logMessage(QStringLiteral("PPT 预览导出完成,共 %1 页").arg(count));
#else
    emit logMessage(QStringLiteral("当前平台不支持 PPT 预览提取(仅 Windows)"));
#endif
}

void MatchController::togglePptPageSelected(int row)
{
    if (m_pptPageModel)
        m_pptPageModel->toggleSelected(row);
}

void MatchController::extractFromSelectedPages()
{
    if (!m_pptPageModel || !m_galleryModel) {
        emit logMessage(QStringLiteral("extractFromSelectedPages: missing model"));
        return;
    }

    const QVector<int> rows = m_pptPageModel->selectedRows();
    if (rows.isEmpty()) {
        emit logMessage(QStringLiteral("extractFromSelectedPages: no pages selected"));
        return;
    }

    QVector<GalleryItem> items;
    constexpr int kStubStylesPerPage = 4;
    items.reserve(rows.size() * kStubStylesPerPage);
    for (int row : rows) {
        const auto *page = m_pptPageModel->at(row);
        if (!page) continue;
        for (int j = 0; j < kStubStylesPerPage; ++j) {
            GalleryItem it;
            it.styleId = QStringLiteral("slide%1_T0JE26B38A%2B")
                             .arg(page->pageIndex, 3, 10, QChar('0'))
                             .arg(j, 3, 10, QChar('0'));
            it.tag = QStringLiteral("baby");
            items.push_back(it);
        }
    }
    m_galleryModel->setItems(std::move(items));
    emit logMessage(QStringLiteral("extractFromSelectedPages: %1 pages -> %2 styles")
                        .arg(rows.size()).arg(items.size()));
}

void MatchController::emitCurrentChanged()
{
    emit currentIndexChanged();
    emit currentImagePageChanged();
    emit currentImageCountChanged();
    emit currentImagePathChanged();
    emit currentStyleIdChanged();
}

void MatchController::loadDemoData()
{
    if (m_candidateModel) {
        QVector<CandidateItem> items;
        for (int i = 0; i < 20; ++i) {
            CandidateItem it;
            it.styleId = QStringLiteral("slide43_T0JE26B38A%1B").arg(i, 3, 10, QChar('0'));
            it.candidateCount = (i % 2) + 1;
            it.score = 0.99 - i * 0.001;
            items.push_back(it);
        }
        m_candidateModel->setItems(std::move(items));
    }
    if (m_galleryModel) {
        QVector<GalleryItem> items;
        const QStringList ids = {
            QStringLiteral("T0JE26B38A008"),
            QStringLiteral("T0YC26B38A110B"),
            QStringLiteral("T0LB26B38A160A"),
            QStringLiteral("T0JE26B38A005"),
        };
        for (const QString &id : ids) {
            GalleryItem it;
            it.styleId = id;
            it.tag = QStringLiteral("baby");
            items.push_back(it);
        }
        m_galleryModel->setItems(std::move(items));
    }
    setCurrentIndex(0);
}

void MatchController::previousImage()
{
    if (m_currentImagePage <= 0)
        return;
    --m_currentImagePage;
    emit currentImagePageChanged();
}

void MatchController::nextImage()
{
    const int total = currentImageCount();
    if (m_currentImagePage + 1 >= total)
        return;
    ++m_currentImagePage;
    emit currentImagePageChanged();
}

void MatchController::openCurrentImageExternally()
{
    const QString p = currentImagePath();
    if (!p.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(p));
}

void MatchController::previousCandidate()
{
    if (m_currentIndex > 0)
        setCurrentIndex(m_currentIndex - 1);
}

void MatchController::nextCandidate()
{
    if (!m_candidateModel)
        return;
    if (m_currentIndex + 1 < m_candidateModel->rowCount())
        setCurrentIndex(m_currentIndex + 1);
}

void MatchController::confirmSelectedThumb(int galleryRow)
{
    emit logMessage(QStringLiteral("confirmSelectedThumb row=%1").arg(galleryRow));
}

void MatchController::confirmStyleId(const QString &styleId)
{
    if (m_candidateModel)
        m_candidateModel->markConfirmed(m_currentIndex, true);
    emit logMessage(QStringLiteral("confirmStyleId=%1").arg(styleId));
}

void MatchController::generateFineTuneModel()
{
    emit logMessage(QStringLiteral("generateFineTuneModel triggered"));
}
