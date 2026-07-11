#include <utility>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLibraryInfo>
#include <QPointer>
#include <QQuickStyle>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include "MatchController.h"
#include "CandidateListModel.h"
#include "GalleryListModel.h"
#include "GarmentMatcher.h"
#include "PhotoListModel.h"
#include "PptPageListModel.h"
#include "PptStyleExtractor.h"

#ifdef Q_OS_WIN
#    include <QAxObject>
#endif

// Short iterator and model names are conventional within this Qt controller implementation.
// NOLINTBEGIN(readability-identifier-length)
namespace
{
    constexpr qint64 kBytesPerKibibyte = 1024;
} // namespace

MatchController::MatchController(QObject *parent)
    : QObject(parent),
      m_availableUiStyles(systemUiStyles()),
      m_currentUiStyle(QQuickStyle::name()),
      m_availableInferenceEngines(GarmentMatcher::availableProviders()),
      m_currentInferenceEngine(GarmentMatcher::activeProvider())
{
    QSettings().setValue(QStringLiteral("matching/provider"), m_currentInferenceEngine.toLower());
}

QStringList MatchController::systemUiStyles()
{
    static const QStringList styles = [] {
        const QDir          controlsDir(QDir(QLibraryInfo::path(QLibraryInfo::QmlImportsPath)).absoluteFilePath(QStringLiteral("QtQuick/Controls")));
        const QFileInfoList directories = controlsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);

        QStringList result;
        for (const QFileInfo &directory : directories)
        {
            const QString name = directory.fileName();
            if (name.compare(QStringLiteral("designer"), Qt::CaseInsensitive) == 0 || name.compare(QStringLiteral("impl"), Qt::CaseInsensitive) == 0)
            {
                continue;
            }
            if (QFileInfo::exists(QDir(directory.absoluteFilePath()).absoluteFilePath(QStringLiteral("qmldir"))))
            {
                result.push_back(name);
            }
        }
        return result;
    }();
    return styles;
}

void MatchController::setCandidateModel(CandidateListModel *m)
{
    m_candidateModel = m;
    if (m_candidateModel)
    {
        m_candidateModel->setFilterText(m_outputFilterText);
    }
}

void MatchController::setGalleryModel(GalleryListModel *m)
{
    m_galleryModel = m;
    if (m_galleryModel)
    {
        m_galleryModel->setFilterText(m_searchText);
    }
}

void MatchController::setPhotoModel(PhotoListModel *m)
{
    m_photoModel = m;
    if (m_photoModel)
    {
        m_photoModel->setFilterText(m_inputFilterText);
    }
}

void MatchController::setPptPageModel(PptPageListModel *m)
{
    if (m_pptPageModel)
    {
        disconnect(m_pptPageModel, nullptr, this, nullptr);
    }
    m_pptPageModel = m;
    if (m_pptPageModel)
    {
        connect(m_pptPageModel, &PptPageListModel::selectedPagesTextChanged, this, &MatchController::persistSelectedPptPages);
    }
}

bool MatchController::setCurrentUiStyle(const QString &style)
{
    QString selectedStyle;
    for (const QString &availableStyle : std::as_const(m_availableUiStyles))
    {
        if (availableStyle.compare(style, Qt::CaseInsensitive) == 0)
        {
            selectedStyle = availableStyle;
            break;
        }
    }
    if (selectedStyle.isEmpty() || selectedStyle.compare(m_currentUiStyle, Qt::CaseInsensitive) == 0)
    {
        return false;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("ui/style"), selectedStyle);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        emit logMessage(QStringLiteral("无法保存界面风格: %1").arg(selectedStyle));
        return false;
    }
    return true;
}

bool MatchController::setCurrentInferenceEngine(const QString &engine)
{
    QString selectedEngine;
    for (const QString &availableEngine : std::as_const(m_availableInferenceEngines))
    {
        if (availableEngine.compare(engine, Qt::CaseInsensitive) == 0)
        {
            selectedEngine = availableEngine;
            break;
        }
    }
    if (selectedEngine.isEmpty())
        return false;

    QSettings settings;
    settings.setValue(QStringLiteral("matching/provider"), selectedEngine.toLower());
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        emit logMessage(QStringLiteral("无法保存推理引擎设置: %1").arg(selectedEngine));
        return false;
    }

    if (selectedEngine == m_currentInferenceEngine)
        return false;

    emit logMessage(QStringLiteral("推理引擎 %1 已保存，重启应用后生效").arg(selectedEngine));
    return true;
}

QString MatchController::title()
{
    return QStringLiteral("Eidos服装款式匹配工作台");
}

int MatchController::currentImageCount() const
{
    if (m_previewSource == PreviewPhoto)
    {
        return m_photoModel && m_currentPhotoIndex >= 0 ? 1 : 0;
    }
    if (!m_candidateModel)
    {
        return 0;
    }
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it ? static_cast<int>(it->imagePaths.size()) : 0;
}

QString MatchController::currentImagePath() const
{
    if (m_previewSource == PreviewPhoto)
    {
        if (!m_photoModel)
        {
            return {};
        }
        const auto *p = m_photoModel->at(m_currentPhotoIndex);
        return p ? p->imagePath : QString();
    }
    if (!m_candidateModel)
    {
        return {};
    }
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it && m_currentImagePage >= 0 && m_currentImagePage < it->imagePaths.size() ? it->imagePaths.at(m_currentImagePage) : QString();
}

QStringList MatchController::currentOutputImagePaths() const
{
    if (!m_candidateModel)
    {
        return {};
    }
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it ? it->imagePaths : QStringList();
}

QString MatchController::currentPhotoPath() const
{
    if (!m_photoModel)
    {
        return {};
    }
    const auto *photo = m_photoModel->at(m_currentPhotoIndex);
    return photo ? photo->imagePath : QString();
}

QString MatchController::currentStyleId() const
{
    if (m_previewSource == PreviewPhoto)
    {
        if (!m_photoModel)
        {
            return {};
        }
        const auto *p = m_photoModel->at(m_currentPhotoIndex);
        return p ? p->fileName : QString();
    }
    if (!m_candidateModel)
    {
        return {};
    }
    const auto *it = m_candidateModel->at(m_currentIndex);
    return it ? it->styleId : QString();
}

void MatchController::setCurrentIndex(int idx)
{
    const bool sourceChanged = (m_previewSource != PreviewOutput);
    if (idx == m_currentIndex && !sourceChanged)
    {
        return;
    }
    const bool indexChanged = idx != m_currentIndex;
    m_currentIndex          = idx;
    m_previewSource         = PreviewOutput;
    if (indexChanged)
    {
        m_currentImagePage = 0;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("selection/outputIndex"), m_currentIndex);
    settings.setValue(QStringLiteral("selection/outputImagePage"), m_currentImagePage);
    settings.setValue(QStringLiteral("preview/inputTabActive"), false);
    emitCurrentChanged();
    if (sourceChanged)
    {
        emit inputTabActiveChanged();
    }
}

void MatchController::setCurrentPhotoIndex(int idx)
{
    const bool sourceChanged = (m_previewSource != PreviewPhoto);
    if (idx == m_currentPhotoIndex && !sourceChanged)
    {
        return;
    }
    m_currentPhotoIndex = idx;
    m_previewSource     = PreviewPhoto;
    if (!m_autoMatchedStyleIds.isEmpty())
    {
        m_autoMatchedStyleIds.clear();
        emit autoMatchedStyleIdsChanged();
    }
    QSettings settings;
    settings.setValue(QStringLiteral("selection/photoIndex"), m_currentPhotoIndex);
    settings.setValue(QStringLiteral("preview/inputTabActive"), true);
    emit currentPhotoIndexChanged();
    emit currentPhotoPathChanged();
    emitCurrentChanged();
    if (sourceChanged)
    {
        emit inputTabActiveChanged();
    }
    emit logMessage(QStringLiteral("selectPhoto row=%1").arg(idx));
}

void MatchController::setCurrentImagePage(int page)
{
    if (page < 0 || page >= currentImageCount() || page == m_currentImagePage)
    {
        return;
    }
    m_currentImagePage = page;
    QSettings().setValue(QStringLiteral("selection/outputImagePage"), m_currentImagePage);
    emit currentImagePageChanged();
    emit currentImagePathChanged();
}

void MatchController::setCategoryFilter(const QString &v)
{
    if (v == m_categoryFilter)
    {
        return;
    }
    m_categoryFilter = v;
    emit categoryFilterChanged();
}

void MatchController::setSearchText(const QString &v)
{
    if (v == m_searchText)
    {
        return;
    }
    m_searchText = v;
    if (m_galleryModel)
    {
        m_galleryModel->setFilterText(v);
    }
    emit searchTextChanged();
}

void MatchController::setInputFilterText(const QString &v)
{
    if (v == m_inputFilterText)
    {
        return;
    }
    m_inputFilterText = v;
    if (m_photoModel)
    {
        m_photoModel->setFilterText(v);
        setCurrentPhotoIndex(m_photoModel->rowCount() > 0 ? 0 : -1);
    }
    emit inputFilterTextChanged();
}

void MatchController::setOutputFilterText(const QString &v)
{
    if (v == m_outputFilterText)
    {
        return;
    }
    m_outputFilterText = v;
    if (m_candidateModel)
    {
        m_candidateModel->setFilterText(v);
        setCurrentIndex(m_candidateModel->rowCount() > 0 ? 0 : -1);
    }
    emit outputFilterTextChanged();
}

void MatchController::setPhotoDir(const QString &v)
{
    if (v == m_photoDir)
    {
        return;
    }
    m_photoDir = v;
    QSettings settings;
    settings.setValue(QStringLiteral("photo/lastDir"), m_photoDir);
    settings.sync();
    emit photoDirChanged();
    emit logMessage(QStringLiteral("photoDir=%1").arg(v));
    scanPhotoDir();
}

void MatchController::setOutputDir(const QString &v)
{
    if (v == m_outputDir)
    {
        return;
    }
    m_outputDir = v;
    QSettings settings;
    settings.setValue(QStringLiteral("output/lastDir"), m_outputDir);
    settings.sync();
    emit outputDirChanged();
    emit logMessage(QStringLiteral("outputDir=%1").arg(v));
    scanOutputDir();
}

void MatchController::setPptPath(const QString &v)
{
    if (v != m_pptPath)
    {
        m_pptPath = v;
        QSettings settings;
        settings.setValue(QStringLiteral("ppt/lastPath"), m_pptPath);
        settings.sync();
        emit pptPathChanged();
        emit logMessage(QStringLiteral("pptPath=%1").arg(v));
    }

    const bool wasRestoring = m_restoringPptState;
    m_restoringPptState     = true;
    auto restoreGuard       = qScopeGuard([this, wasRestoring] { m_restoringPptState = wasRestoring; });
    loadPptPreviewsFromCache();
    restoreSelectedPptPages();
    loadPptStylesFromCache();
}

QString MatchController::pptCacheDir(const QString &pptFilePath)
{
    const QFileInfo fi(pptFilePath);
    if (!fi.exists() || !fi.isFile())
    {
        return {};
    }
    const QString    key    = fi.fileName() + QLatin1Char(':') + QString::number(fi.size());
    const QByteArray hash   = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1);
    const QString    subDir = QString::fromLatin1(hash.toHex().left(16));
    const QString    base   = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return base + QStringLiteral("/ppt_slides/") + subDir;
}

QString MatchController::pptPagesSettingsKey(const QString &pptFilePath)
{
    QString normalizedPath = QDir::fromNativeSeparators(QFileInfo(pptFilePath).absoluteFilePath());
#ifdef Q_OS_WIN
    normalizedPath = normalizedPath.toCaseFolded();
#endif
    const QByteArray hash = QCryptographicHash::hash(normalizedPath.toUtf8(), QCryptographicHash::Sha1);
    return QStringLiteral("ppt/selectedPages/%1").arg(QString::fromLatin1(hash.toHex()));
}

void MatchController::persistSelectedPptPages()
{
    if (m_restoringPptState || !m_pptPageModel || m_pptPath.isEmpty())
    {
        return;
    }
    QSettings settings;
    settings.setValue(pptPagesSettingsKey(m_pptPath), m_pptPageModel->selectedPagesText());
    settings.sync();
}

void MatchController::restoreSelectedPptPages()
{
    if (!m_pptPageModel || m_pptPath.isEmpty())
    {
        return;
    }
    const QString pages        = QSettings().value(pptPagesSettingsKey(m_pptPath)).toString();
    const bool    wasRestoring = m_restoringPptState;
    m_restoringPptState        = true;
    m_pptPageModel->setSelectedPagesText(pages);
    m_restoringPptState = wasRestoring;
}

bool MatchController::loadPptPreviewsFromCache()
{
    if (!m_pptPageModel)
    {
        return false;
    }
    m_pptPageModel->clear();
    if (m_pptPath.isEmpty())
    {
        return false;
    }

    const QString dir = pptCacheDir(m_pptPath);
    if (dir.isEmpty())
    {
        return false;
    }
    QDir d(dir);
    if (!d.exists())
    {
        return false;
    }
    const auto files = d.entryInfoList({QStringLiteral("slide_*.png")}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    if (files.isEmpty())
    {
        return false;
    }

    for (int i = 0; i < files.size(); ++i)
    {
        PptPageItem p;
        p.pageIndex = i + 1;
        p.imagePath = files.at(i).absoluteFilePath();
        m_pptPageModel->appendItem(p);
    }
    emit logMessage(QStringLiteral("从缓存加载 %1 页预览: %2").arg(files.size()).arg(dir));
    return true;
}

bool MatchController::loadPptStylesFromCache()
{
    if (!m_galleryModel)
    {
        return false;
    }

    const QString cacheRoot = pptCacheDir(m_pptPath);
    const QString stylesDir = cacheRoot.isEmpty() ? QString() : QDir(cacheRoot).absoluteFilePath(QStringLiteral("styles"));
    m_galleryModel->loadFromStyleCacheDir(stylesDir);

    const int count = m_galleryModel->rowCount();
    if (count > 0)
    {
        emit logMessage(QStringLiteral("已从缓存载入 %1 张手绘图: %2").arg(count).arg(stylesDir));
    }
    return count > 0;
}

void MatchController::scanPhotoDir()
{
    if (!m_photoModel)
    {
        emit logMessage(QStringLiteral("scanPhotoDir: no photoModel"));
        return;
    }

    QVector<PhotoItem> items;
    if (!m_photoDir.isEmpty())
    {
        const QDir               photoDirectory(m_photoDir);
        static const QStringList imgFilter = {
            QStringLiteral("*.jpg"),
            QStringLiteral("*.jpeg"),
        };
        QDirIterator it(m_photoDir, imgFilter, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            it.next();
            const QFileInfo fi = it.fileInfo();
            PhotoItem       p;
            p.fileName     = fi.fileName();
            p.imagePath    = fi.absoluteFilePath();
            p.relativePath = photoDirectory.relativeFilePath(p.imagePath);
            items.push_back(p);
        }
        std::sort(items.begin(), items.end(), [](const PhotoItem &a, const PhotoItem &b) { return a.imagePath < b.imagePath; });
    }
    m_photoModel->setItems(std::move(items));
    setCurrentPhotoIndex(m_photoModel->rowCount() > 0 ? 0 : -1);
    emit currentPhotoPathChanged();
    emit logMessage(QStringLiteral("scanPhotoDir=%1 (%2 files)").arg(m_photoDir).arg(m_photoModel->rowCount()));
}

void MatchController::scanOutputDir()
{
    if (!m_candidateModel)
    {
        return;
    }

    QVector<CandidateItem> items;
    if (!m_outputDir.isEmpty())
    {
        static const QStringList imgFilter = {
            QStringLiteral("*.png"),
            QStringLiteral("*.jpg"),
            QStringLiteral("*.jpeg"),
            QStringLiteral("*.bmp"),
            QStringLiteral("*.webp"),
        };
        QDir       dir(m_outputDir);
        const auto directories = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        items.reserve(directories.size());
        for (const QFileInfo &directory : directories)
        {
            const QDir    styleDir(directory.absoluteFilePath());
            const auto    images = styleDir.entryInfoList(imgFilter, QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
            CandidateItem it;
            it.styleId = directory.fileName();
            it.imagePaths.reserve(images.size());
            for (const QFileInfo &image : images)
            {
                it.imagePaths.push_back(image.absoluteFilePath());
            }
            it.imagePath      = it.imagePaths.isEmpty() ? QString() : it.imagePaths.constFirst();
            it.candidateCount = static_cast<int>(it.imagePaths.size());
            it.score          = 0.0;
            it.confirmed      = false;
            items.push_back(it);
        }
    }
    m_candidateModel->setItems(std::move(items));
    setCurrentIndex(m_candidateModel->rowCount() > 0 ? 0 : -1);
    emit logMessage(QStringLiteral("scanOutputDir=%1 (%2 directories)").arg(m_outputDir).arg(m_candidateModel->rowCount()));
}

void MatchController::setBusy(bool on)
{
    if (m_busy == on)
    {
        return;
    }
    m_busy = on;
    emit busyChanged();
}

void MatchController::reloadPpt()
{
    emit logMessage(QStringLiteral("reloadPpt=%1").arg(m_pptPath));
    if (!m_pptPageModel)
    {
        return;
    }

    setBusy(true);
    auto busyGuard = qScopeGuard([this] { setBusy(false); });

    {
        const bool wasRestoring = m_restoringPptState;
        m_restoringPptState     = true;
        auto restoreGuard       = qScopeGuard([this, wasRestoring] { m_restoringPptState = wasRestoring; });
        m_pptPageModel->clear();
        restoreSelectedPptPages();
    }

    if (m_pptPath.isEmpty())
    {
        return;
    }

    const QFileInfo fi(m_pptPath);
    if (!fi.exists() || !fi.isFile())
    {
        emit logMessage(QStringLiteral("PPT 文件不存在: %1").arg(m_pptPath));
        return;
    }

#ifdef Q_OS_WIN
    const QString cacheDir = pptCacheDir(m_pptPath);
    if (cacheDir.isEmpty())
    {
        emit logMessage(QStringLiteral("无法确定缓存目录"));
        return;
    }
    QDir dir(cacheDir);
    if (!dir.exists())
    {
        QDir().mkpath(cacheDir);
    }
    const auto stale = dir.entryInfoList({QStringLiteral("slide_*.png")}, QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &f : stale)
    {
        QFile::remove(f.absoluteFilePath());
    }

    QAxObject ppApp(QStringLiteral("PowerPoint.Application"));
    if (ppApp.isNull())
    {
        emit logMessage(QStringLiteral("PowerPoint COM 启动失败,请确认已安装 Microsoft PowerPoint"));
        return;
    }

    QAxObject *presentations = ppApp.querySubObject("Presentations");
    if (!presentations)
    {
        emit logMessage(QStringLiteral("获取 Presentations 集合失败"));
        ppApp.dynamicCall("Quit()");
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
    // Open(FileName, ReadOnly=msoTrue, Untitled=msoFalse, WithWindow=msoFalse)
    QAxObject *pres = presentations->querySubObject("Open(const QString&,int,int,int)", nativePath, -1, 0, 0);
    if (!pres)
    {
        emit logMessage(QStringLiteral("PPT 打开失败: %1").arg(nativePath));
        delete presentations;
        ppApp.dynamicCall("Quit()");
        return;
    }

    QAxObject *slides = pres->querySubObject("Slides");
    const int  count  = slides ? slides->property("Count").toInt() : 0;
    emit       logMessage(QStringLiteral("PPT 共 %1 页,开始导出预览...").arg(count));

    constexpr int kExportW = 800;
    constexpr int kExportH = 600;
    for (int i = 1; i <= count; ++i)
    {
        QAxObject *slide = slides->querySubObject("Item(int)", i);
        if (!slide)
        {
            continue;
        }
        const QString outPath = QDir::toNativeSeparators(cacheDir + QStringLiteral("/slide_%1.png").arg(i, 4, 10, QChar('0')));
        slide->dynamicCall("Export(const QString&,const QString&,int,int)", outPath, QStringLiteral("PNG"), kExportW, kExportH);
        delete slide;

        const QFileInfo outFi(outPath);
        if (!outFi.exists() || outFi.size() <= 0)
        {
            emit logMessage(QStringLiteral("警告: slide %1 导出失败或文件为空: %2").arg(i).arg(outPath));
            QCoreApplication::processEvents();
            continue;
        }

        PptPageItem p;
        p.pageIndex = i;
        p.imagePath = QDir::fromNativeSeparators(outPath);
        m_pptPageModel->appendItem(p);

        emit logMessage(QStringLiteral("导出 slide %1/%2 (%3 KB, rowCount=%4)")
                            .arg(i)
                            .arg(count)
                            .arg(outFi.size() / kBytesPerKibibyte)
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
    {
        m_pptPageModel->toggleSelected(row);
    }
}

void MatchController::extractFromSelectedPages() // NOLINT(readability-function-cognitive-complexity)
{
    if (!m_pptPageModel || !m_galleryModel)
    {
        emit logMessage(QStringLiteral("extractFromSelectedPages: missing model"));
        return;
    }
    if (m_busy)
    {
        emit logMessage(QStringLiteral("款号和手绘图正在提取，请等待当前任务完成"));
        return;
    }

    const QVector<int> rows = m_pptPageModel->selectedRows();
    if (rows.isEmpty())
    {
        emit logMessage(QStringLiteral("extractFromSelectedPages: no pages selected"));
        return;
    }

    const QFileInfo fi(m_pptPath);
    if (m_pptPath.isEmpty() || !fi.isFile())
    {
        emit logMessage(QStringLiteral("PPT 文件不存在: %1").arg(m_pptPath));
        return;
    }
    if (fi.suffix().toLower() != QStringLiteral("pptx"))
    {
        emit logMessage(QStringLiteral("仅支持 .pptx (Open XML); 当前为 .%1").arg(fi.suffix()));
        return;
    }

    const QString cacheRoot = pptCacheDir(m_pptPath);
    if (cacheRoot.isEmpty())
    {
        emit logMessage(QStringLiteral("无法确定缓存目录"));
        return;
    }
    const QString stylesDir = cacheRoot + QStringLiteral("/styles");

    QVector<int> pages;
    pages.reserve(rows.size());
    for (int row : rows)
    {
        const auto *page = m_pptPageModel->at(row);
        if (page)
        {
            pages.push_back(page->pageIndex);
        }
    }

    PptStyleExtractor::Options opts;
    opts.pptxPath   = m_pptPath;
    opts.pages      = pages;
    opts.outputDir  = stylesDir;
    opts.openXmlDir = cacheRoot + QStringLiteral("/openxml");
    const QPointer<MatchController> guard(this);
    opts.progress = [guard](int current, int total, const QString &detail) {
        if (!guard)
        {
            return;
        }
        const QString message = total > 0 ? QStringLiteral("提取进度 %1/%2｜%3").arg(current).arg(total).arg(detail) : detail;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, message] {
                if (guard)
                {
                    emit guard->logMessage(message);
                }
            },
            Qt::QueuedConnection);
    };

    auto *watcher = new QFutureWatcher<PptStyleExtractor::Result>(this);
    connect(watcher,
            &QFutureWatcher<PptStyleExtractor::Result>::finished,
            this,
            [this, watcher, pageCount = static_cast<int>(pages.size()), sourcePptPath = m_pptPath, stylesDir] {
                const PptStyleExtractor::Result res = watcher->result();
                watcher->deleteLater();

                if (m_pptPath != sourcePptPath)
                {
                    setBusy(false);
                    emit logMessage(QStringLiteral("后台提取已完成，但当前 PPT 已切换，未替换款号小图库"));
                    return;
                }

                for (const QString &warning : res.warnings)
                {
                    emit logMessage(warning);
                }

                qsizetype imageCount = 0;
                for (const auto &style : res.styles)
                {
                    imageCount += style.imagePaths.size();
                }

                QVector<GalleryItem> items;
                items.reserve(imageCount);
                for (const auto &style : res.styles)
                {
                    for (const QString &imagePath : style.imagePaths)
                    {
                        GalleryItem item;
                        item.styleId   = style.styleId;
                        item.imagePath = imagePath;
                        item.tag       = QStringLiteral("baby");
                        items.push_back(std::move(item));
                    }
                }
                m_galleryModel->setItems(std::move(items));
                setBusy(false);

                emit logMessage(QStringLiteral("提取完成：从 %1 页提取到 %2 个款式、%3 张手绘图，已保存到 %4")
                                    .arg(pageCount)
                                    .arg(res.styles.size())
                                    .arg(imageCount)
                                    .arg(stylesDir));
            });

    setBusy(true);
    emit logMessage(QStringLiteral("提取进度 0/%1｜正在启动后台提取任务...").arg(pages.size()));
    watcher->setFuture(QtConcurrent::run([opts] { return PptStyleExtractor::extract(opts); }));
}

void MatchController::emitCurrentChanged()
{
    emit currentIndexChanged();
    emit currentImagePageChanged();
    emit currentImageCountChanged();
    emit currentImagePathChanged();
    emit currentOutputImagePathsChanged();
    emit currentStyleIdChanged();
}

void MatchController::loadDemoData()
{
    if (m_galleryModel)
    {
        QVector<GalleryItem> items;
        const QStringList    ids = {
            QStringLiteral("T0JE26B38A008"),
            QStringLiteral("T0YC26B38A110B"),
            QStringLiteral("T0LB26B38A160A"),
            QStringLiteral("T0JE26B38A005"),
        };
        for (const QString &id : ids)
        {
            GalleryItem it;
            it.styleId = id;
            it.tag     = QStringLiteral("baby");
            items.push_back(it);
        }
        m_galleryModel->setItems(std::move(items));
    }
}

void MatchController::restorePersistentState()
{
    const QSettings settings;
    const QString   lastPhotoDir        = settings.value(QStringLiteral("photo/lastDir")).toString();
    const QString   lastOutputDir       = settings.value(QStringLiteral("output/lastDir")).toString();
    const QString   lastPptPath         = settings.value(QStringLiteral("ppt/lastPath")).toString();
    const int       lastPhotoIndex      = settings.value(QStringLiteral("selection/photoIndex"), -1).toInt();
    const int       lastOutputIndex     = settings.value(QStringLiteral("selection/outputIndex"), -1).toInt();
    const int       lastOutputImagePage = settings.value(QStringLiteral("selection/outputImagePage"), 0).toInt();
    const bool      lastInputTabActive  = settings.value(QStringLiteral("preview/inputTabActive"), true).toBool();
    if (!lastPhotoDir.isEmpty())
    {
        setPhotoDir(lastPhotoDir);
    }
    if (!lastOutputDir.isEmpty())
    {
        setOutputDir(lastOutputDir);
    }
    if (!lastPptPath.isEmpty())
    {
        setPptPath(lastPptPath);
    }
    if (m_photoModel && lastPhotoIndex >= 0 && lastPhotoIndex < m_photoModel->rowCount())
    {
        setCurrentPhotoIndex(lastPhotoIndex);
    }
    if (m_candidateModel && lastOutputIndex >= 0 && lastOutputIndex < m_candidateModel->rowCount())
    {
        setCurrentIndex(lastOutputIndex);
    }
    if (lastOutputImagePage >= 0 && lastOutputImagePage < currentImageCount())
    {
        setCurrentImagePage(lastOutputImagePage);
    }
    activatePreview(lastInputTabActive);
}

void MatchController::activatePreview(bool inputTabActive)
{
    if (inputTabActive)
    {
        setCurrentPhotoIndex(m_currentPhotoIndex);
    }
    else
    {
        setCurrentIndex(m_currentIndex);
    }
}

void MatchController::previousImage(bool inputTabActive)
{
    if (inputTabActive && m_currentPhotoIndex >= 0)
    {
        if (m_currentPhotoIndex > 0)
        {
            setCurrentPhotoIndex(m_currentPhotoIndex - 1);
        }
        return;
    }

    if (m_currentImagePage <= 0)
    {
        return;
    }
    --m_currentImagePage;
    emit currentImagePageChanged();
    emit currentImagePathChanged();
}

void MatchController::nextImage(bool inputTabActive)
{
    if (inputTabActive && m_currentPhotoIndex >= 0)
    {
        if (m_photoModel && m_currentPhotoIndex + 1 < m_photoModel->rowCount())
        {
            setCurrentPhotoIndex(m_currentPhotoIndex + 1);
        }
        return;
    }

    const int total = currentImageCount();
    if (m_currentImagePage + 1 >= total)
    {
        return;
    }
    ++m_currentImagePage;
    emit currentImagePageChanged();
    emit currentImagePathChanged();
}

void MatchController::openCurrentImageExternally() const
{
    const QString p = currentImagePath();
    if (!p.isEmpty())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(p));
    }
}

void MatchController::previousCandidate()
{
    if (m_currentIndex > 0)
    {
        setCurrentIndex(m_currentIndex - 1);
    }
}

void MatchController::nextCandidate()
{
    if (!m_candidateModel)
    {
        return;
    }
    if (m_currentIndex + 1 < m_candidateModel->rowCount())
    {
        setCurrentIndex(m_currentIndex + 1);
    }
}

void MatchController::confirmSelectedThumb(int galleryRow)
{
    emit logMessage(QStringLiteral("confirmSelectedThumb row=%1").arg(galleryRow));
}

void MatchController::confirmStyleId(const QString &styleId)
{
    if (m_candidateModel)
    {
        m_candidateModel->markConfirmed(m_currentIndex, true);
    }
    emit logMessage(QStringLiteral("confirmStyleId=%1").arg(styleId));
}

void MatchController::autoMatchStyleIds()
{
    if (m_busy)
    {
        emit logMessage(QStringLiteral("已有后台任务正在运行，请等待完成"));
        return;
    }
    if (!m_galleryModel)
    {
        emit logMessage(QStringLiteral("自动匹配失败：款号小图库模型不可用"));
        return;
    }

    const QString photoPath = currentPhotoPath();
    if (photoPath.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：请先选择一张实拍图"));
        return;
    }
    const QVector<GalleryItem> galleryItems = m_galleryModel->allItems();
    if (galleryItems.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：请先从 PPT 提取款号小图库"));
        return;
    }

    const QString           modelsDir = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("models"));
    const QSettings         settings;
    GarmentMatcher::Options options;
    options.segmentationModelPath =
        settings.value(QStringLiteral("matching/segmentationModel"), QDir(modelsDir).absoluteFilePath(QStringLiteral("clothes_segformer_b2.onnx")))
            .toString();
    options.embeddingModelPath =
        settings.value(QStringLiteral("matching/embeddingModel"), QDir(modelsDir).absoluteFilePath(QStringLiteral("fashion_clip_vision.onnx")))
            .toString();
    options.featureDatabasePath =
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).absoluteFilePath(QStringLiteral("style_embeddings.sqlite"));

    auto *watcher = new QFutureWatcher<GarmentMatcher::Result>(this);
    connect(watcher, &QFutureWatcher<GarmentMatcher::Result>::finished, this, [this, watcher, photoPath] {
        const GarmentMatcher::Result result = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        if (currentPhotoPath() != photoPath)
        {
            emit logMessage(QStringLiteral("自动匹配已完成，但当前实拍图已切换，结果未回填"));
            return;
        }
        if (!result.success)
        {
            emit logMessage(QStringLiteral("自动匹配失败：%1").arg(result.error));
            return;
        }

        const QString styleIds = result.joinedStyleIds();
        if (styleIds != m_autoMatchedStyleIds)
        {
            m_autoMatchedStyleIds = styleIds;
            emit autoMatchedStyleIdsChanged();
        }
        emit logMessage(QStringLiteral("自动匹配完成（%1）：上衣 %2 (%3)，下装 %4 (%5)")
                            .arg(result.provider,
                                 result.upper.styleId.isEmpty() ? QStringLiteral("未检出") : result.upper.styleId,
                                 QString::number(result.upper.score, 'f', 3),
                                 result.lower.styleId.isEmpty() ? QStringLiteral("未检出") : result.lower.styleId,
                                 QString::number(result.lower.score, 'f', 3)));
    });

    setBusy(true);
    emit logMessage(QStringLiteral("正在分割服装并匹配 %1 张款号图片...").arg(galleryItems.size()));
    watcher->setFuture(QtConcurrent::run([photoPath, galleryItems, options] { return GarmentMatcher::match(photoPath, galleryItems, options); }));
}

void MatchController::generateFineTuneModel()
{
    emit logMessage(QStringLiteral("generateFineTuneModel triggered"));
}
// NOLINTEND(readability-identifier-length)
