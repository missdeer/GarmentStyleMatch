#include <algorithm>
#include <array>
#include <utility>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLibraryInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQuickStyle>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>
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
    constexpr qint64 kBytesPerKibibyte        = 1024;
    constexpr qint64 kModelDownloadSize       = 512778784;
    constexpr int    kMaxParallelMatchThreads = 8;

    struct ModelDownload
    {
        QString    fileName;
        QUrl       url;
        QByteArray sha256;
        qint64     size;
    };

    const std::array<ModelDownload, 2> &modelDownloads()
    {
        static const std::array<ModelDownload, 2> downloads = {
            ModelDownload {QStringLiteral("clothes_segformer_b2.onnx"),
                           QUrl(QStringLiteral("https://huggingface.co/mattmdjaga/segformer_b2_clothes/resolve/main/onnx/model.onnx")),
                           QByteArrayLiteral("a93a8dac171b5c1fcc53632a8bfc180bfd9759ea69a3e207451bb07f76add54f"),
                           110039290},
            ModelDownload {QStringLiteral("fashion_clip.onnx"),
                           QUrl(QStringLiteral("https://huggingface.co/patrickjohncyh/fashion-clip/resolve/main/onnx/model.onnx")),
                           QByteArrayLiteral("dc4c724479e49d1da9598969125353113a341bd4fd5a1dbc7d528d3f1545bba9"),
                           402739494},
        };
        return downloads;
    }

    QString modelSetupDir()
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .absoluteFilePath(QStringLiteral("GarmentStyleMatch-qt-model-setup"));
    }

    bool fileMatchesSha256(const QString &path, const QByteArray &expected)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file))
        {
            return false;
        }
        return hash.result().toHex() == expected;
    }

    bool hasConfirmedMatch(const StoredMatchResult &result, const QString &part)
    {
        const auto confirmed = [](const StoredGarmentMatch &match) { return !match.isEmpty() && match.confirmed; };
        return part == QLatin1String("all")     ? confirmed(result.upper) || confirmed(result.lower)
               : part == QLatin1String("upper") ? confirmed(result.upper)
               : part == QLatin1String("lower") ? confirmed(result.lower)
                                                : false;
    }

    GarmentMatcher::Options matcherOptions(const QString &modelsDir)
    {
        GarmentMatcher::Options options;
        options.segmentationModelPath = QDir(modelsDir).absoluteFilePath(QStringLiteral("clothes_segformer_b2.onnx"));
        options.embeddingModelPath    = QDir(modelsDir).absoluteFilePath(QStringLiteral("fashion_clip_vision.onnx"));
        options.featureDatabasePath =
            QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).absoluteFilePath(QStringLiteral("style_embeddings.sqlite"));
        return options;
    }

    StoredMatchResult storedMatchResult(const GarmentMatcher::Result &result)
    {
        StoredMatchResult stored;
        stored.upper = {result.upper.styleId, QFileInfo(result.upper.imagePath).fileName(), false};
        stored.lower = {result.lower.styleId, QFileInfo(result.lower.imagePath).fileName(), false};
        return stored;
    }

    PhotoMatchStatus photoMatchStatus(const StoredGarmentMatch &match)
    {
        return match.isEmpty() ? PhotoMatchStatus::Unmatched : match.confirmed ? PhotoMatchStatus::Confirmed : PhotoMatchStatus::Matched;
    }

    struct BatchAutoMatchSummary
    {
        int     succeeded             = 0;
        int     skipped               = 0;
        int     failed                = 0;
        int     unprocessed           = 0;
        bool    cancelled             = false;
        bool    modelDownloadRequired = false;
        QString firstError;
    };

    [[nodiscard]] int recommendedParallelMatchThreadCount()
    {
        return 1;
    }
} // namespace

MatchController::MatchController(QObject *parent)
    : QObject(parent),
      m_availableUiStyles(systemUiStyles()),
      m_currentUiStyle(QQuickStyle::name()),
      m_availableInferenceEngines(GarmentMatcher::availableProviders()),
      m_currentInferenceEngine(GarmentMatcher::activeProvider())
{
    QSettings settings;
    settings.setValue(QStringLiteral("matching/provider"), m_currentInferenceEngine.toLower());
    m_parallelMatchThreadCount = settings.value(QStringLiteral("matching/parallelThreads"), recommendedParallelMatchThreadCount()).toInt();
    m_parallelMatchThreadCount = std::clamp(m_parallelMatchThreadCount, 1, kMaxParallelMatchThreads);
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
    if (m_galleryModel)
    {
        disconnect(m_galleryModel, nullptr, this, nullptr);
    }
    m_galleryModel = m;
    if (m_galleryModel)
    {
        m_galleryModel->setFilterText(m_searchText);
        connect(m_galleryModel, &GalleryListModel::countChanged, this, &MatchController::rebuildAutoMatchedItems);
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
    {
        return false;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("matching/provider"), selectedEngine.toLower());
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        emit logMessage(QStringLiteral("无法保存推理引擎设置: %1").arg(selectedEngine));
        return false;
    }

    if (selectedEngine == m_currentInferenceEngine)
    {
        return false;
    }

    emit logMessage(QStringLiteral("推理引擎 %1 已保存，重启应用后生效").arg(selectedEngine));
    return true;
}

void MatchController::setParallelMatchThreadCount(int count)
{
    if (m_busy || count < 1 || count > kMaxParallelMatchThreads || count == m_parallelMatchThreadCount)
    {
        return;
    }
    m_parallelMatchThreadCount = count;
    QSettings().setValue(QStringLiteral("matching/parallelThreads"), count);
    emit parallelMatchThreadCountChanged();
    emit logMessage(QStringLiteral("并行匹配线程数已设置为 %1").arg(count));
}

QString MatchController::modelDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).absoluteFilePath(QStringLiteral("models"));
}

QString MatchController::applicationModelDirectory()
{
    const QDir applicationDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_MACOS
    return QDir::cleanPath(applicationDir.absoluteFilePath(QStringLiteral("../Resources/models")));
#else
    return applicationDir.absoluteFilePath(QStringLiteral("models"));
#endif
}

QString MatchController::findAvailableModelDirectory(const QString &applicationModelsDir, const QString &localModelsDir)
{
    const auto hasRequiredModels = [](const QString &directory) {
        const QDir models(directory);
        return QFileInfo(models.absoluteFilePath(QStringLiteral("clothes_segformer_b2.onnx"))).isFile() &&
               QFileInfo(models.absoluteFilePath(QStringLiteral("fashion_clip_vision.onnx"))).isFile();
    };
    if (hasRequiredModels(applicationModelsDir))
    {
        return applicationModelsDir;
    }
    return hasRequiredModels(localModelsDir) ? localModelsDir : QString();
}

QString MatchController::availableModelDirectory()
{
    return findAvailableModelDirectory(applicationModelDirectory(), modelDirectory());
}

bool MatchController::modelsAvailable()
{
    const QDir models(modelDirectory());
    return QFileInfo(models.absoluteFilePath(QStringLiteral("clothes_segformer_b2.onnx"))).isFile() &&
           QFileInfo(models.absoluteFilePath(QStringLiteral("fashion_clip_vision.onnx"))).isFile();
}

void MatchController::downloadModels()
{
    if (m_modelDownloadInProgress)
    {
        return;
    }

    const QString packagesDir = QDir(modelSetupDir()).absoluteFilePath(QStringLiteral("packages"));
    m_pythonPackagesDir       = QDir(modelSetupDir()).absoluteFilePath(QStringLiteral("python-packages"));
    if (!QDir().mkpath(packagesDir) || !QDir().mkpath(m_pythonPackagesDir))
    {
        emit logMessage(QStringLiteral("下载模型失败：无法创建模型目录"));
        return;
    }

    m_modelDownloadInProgress            = true;
    m_modelDownloadCancellationRequested = false;
    m_modelDownloadIndex                 = 0;
    m_modelDownloadBytesCompleted        = 0;
    m_modelDownloadError.clear();
    emit modelDownloadInProgressChanged();
    emit logMessage(QStringLiteral("正在下载并校验模型，首次下载可能需要较长时间..."));
    startNextModelDownload();
}

void MatchController::cancelModelDownload()
{
    if (!m_modelDownloadInProgress || m_modelDownloadCancellationRequested)
    {
        return;
    }

    m_modelDownloadCancellationRequested = true;
    emit logMessage(QStringLiteral("正在停止模型下载..."));
    if (m_modelDownloadReply)
    {
        m_modelDownloadReply->abort();
    }
    else if (m_modelDownloadProcess)
    {
        m_modelDownloadProcess->kill();
    }
    else
    {
        finishModelDownload(QStringLiteral("模型下载已停止"));
    }
}

void MatchController::startNextModelDownload() // NOLINT(readability-function-cognitive-complexity)
{
    const auto &downloads = modelDownloads();
    const QDir  packagesDir(QDir(modelSetupDir()).absoluteFilePath(QStringLiteral("packages")));
    while (m_modelDownloadIndex < static_cast<int>(downloads.size()))
    {
        const ModelDownload &download = downloads.at(m_modelDownloadIndex);
        if (!fileMatchesSha256(packagesDir.absoluteFilePath(download.fileName), download.sha256))
        {
            break;
        }
        m_modelDownloadBytesCompleted += download.size;
        ++m_modelDownloadIndex;
    }

    if (m_modelDownloadIndex == static_cast<int>(downloads.size()))
    {
        if (!QDir().mkpath(modelDirectory()))
        {
            finishModelDownload(QStringLiteral("模型准备失败：无法创建模型保存目录"));
            return;
        }
        const QString source = packagesDir.absoluteFilePath(QStringLiteral("clothes_segformer_b2.onnx"));
        const QString target = QDir(modelDirectory()).absoluteFilePath(QStringLiteral("clothes_segformer_b2.onnx"));
        QFile::remove(target);
        if (!QFile::copy(source, target))
        {
            finishModelDownload(QStringLiteral("模型准备失败：无法保存服装分割模型"));
            return;
        }
        startPythonDependencyInstall();
        return;
    }

    const ModelDownload &download = downloads.at(m_modelDownloadIndex);
    const QString        path     = packagesDir.absoluteFilePath(download.fileName);
    QFile::remove(path);
    auto *file = new QFile(path);
    if (!file->open(QIODevice::WriteOnly))
    {
        const QString error = file->errorString();
        delete file;
        finishModelDownload(QStringLiteral("下载模型失败：%1").arg(error));
        return;
    }

    if (!m_modelDownloadNetworkManager)
    {
        m_modelDownloadNetworkManager = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(download.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GarmentStyleMatch/%1").arg(QCoreApplication::applicationVersion()));
    auto *reply          = m_modelDownloadNetworkManager->get(request);
    m_modelDownloadReply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, file] {
        const QByteArray data = reply->readAll();
        if (file->write(data) != data.size() && m_modelDownloadError.isEmpty())
        {
            m_modelDownloadError = QStringLiteral("写入模型临时文件失败：%1").arg(file->errorString());
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, download](qint64 received, qint64) {
        const qint64 totalReceived = m_modelDownloadBytesCompleted + received;
        const int    percent       = static_cast<int>(qBound<qint64>(qint64 {0}, totalReceived * 100 / kModelDownloadSize, qint64 {100}));
        emit         logMessage(QStringLiteral("正在下载模型：%1 %2%").arg(download.fileName).arg(percent));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, path, download] {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty() && file->write(remaining) != remaining.size() && m_modelDownloadError.isEmpty())
        {
            m_modelDownloadError = QStringLiteral("写入模型临时文件失败：%1").arg(file->errorString());
        }
        file->close();
        m_modelDownloadReply = nullptr;
        file->deleteLater();
        const QNetworkReply::NetworkError networkError     = reply->error();
        const QString                     networkErrorText = reply->errorString();
        reply->deleteLater();

        if (m_modelDownloadCancellationRequested)
        {
            QFile::remove(path);
            finishModelDownload(QStringLiteral("模型下载已停止"));
            return;
        }
        if (!m_modelDownloadError.isEmpty() || networkError != QNetworkReply::NoError)
        {
            QFile::remove(path);
            finishModelDownload(m_modelDownloadError.isEmpty() ? QStringLiteral("下载模型失败：%1").arg(networkErrorText)
                                                               : QStringLiteral("下载模型失败：%1").arg(m_modelDownloadError));
            return;
        }
        if (!fileMatchesSha256(path, download.sha256))
        {
            QFile::remove(path);
            finishModelDownload(QStringLiteral("下载模型失败：%1 的 SHA-256 校验不匹配").arg(download.fileName));
            return;
        }

        m_modelDownloadBytesCompleted += download.size;
        ++m_modelDownloadIndex;
        m_modelDownloadError.clear();
        startNextModelDownload();
    });
}

void MatchController::startPythonDependencyInstall()
{
#ifdef Q_OS_WIN
    const QStringList candidates = {QStringLiteral("python.exe"), QStringLiteral("python3.exe")};
#else
    const QStringList candidates = {QStringLiteral("python3"), QStringLiteral("python")};
#endif
    for (const QString &candidate : candidates)
    {
        m_pythonExecutable = QStandardPaths::findExecutable(candidate);
        if (!m_pythonExecutable.isEmpty())
        {
            break;
        }
    }
    if (m_pythonExecutable.isEmpty())
    {
        finishModelDownload(QStringLiteral("模型提取失败：找不到 Python"));
        return;
    }

    if (QFileInfo::exists(QDir(m_pythonPackagesDir).absoluteFilePath(QStringLiteral("onnx/__init__.py"))))
    {
        startPythonModelExtraction();
        return;
    }

    emit  logMessage(QStringLiteral("正在准备模型提取所需的 Python onnx 包..."));
    auto *process          = new QProcess(this);
    m_modelDownloadProcess = process;
    connectModelProcessOutput(process);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        m_modelDownloadProcess = nullptr;
        process->deleteLater();
        if (m_modelDownloadCancellationRequested)
        {
            finishModelDownload(QStringLiteral("模型下载已停止"));
        }
        else if (exitStatus != QProcess::NormalExit || exitCode != 0)
        {
            finishModelDownload(QStringLiteral("模型提取失败：无法准备 Python onnx 包"));
        }
        else
        {
            startPythonModelExtraction();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            m_modelDownloadProcess = nullptr;
            const QString message  = process->errorString();
            process->deleteLater();
            finishModelDownload(QStringLiteral("模型提取失败：%1").arg(message));
        }
    });
    process->start(m_pythonExecutable,
                   {QStringLiteral("-m"),
                    QStringLiteral("pip"),
                    QStringLiteral("install"),
                    QStringLiteral("--disable-pip-version-check"),
                    QStringLiteral("--target"),
                    m_pythonPackagesDir,
                    QStringLiteral("onnx==1.19.1")});
}

void MatchController::startPythonModelExtraction()
{
    emit          logMessage(QStringLiteral("正在提取 FashionCLIP 图像模型..."));
    const QDir    packagesDir(QDir(modelSetupDir()).absoluteFilePath(QStringLiteral("packages")));
    const QString source    = packagesDir.absoluteFilePath(QStringLiteral("fashion_clip.onnx"));
    const QString extracted = packagesDir.absoluteFilePath(QStringLiteral("fashion_clip_vision.onnx"));
    QFile::remove(extracted);

    auto *process          = new QProcess(this);
    m_modelDownloadProcess = process;
    connectModelProcessOutput(process);
    QProcessEnvironment environment        = QProcessEnvironment::systemEnvironment();
    const QString       existingPythonPath = environment.value(QStringLiteral("PYTHONPATH"));
    environment.insert(QStringLiteral("PYTHONPATH"),
                       existingPythonPath.isEmpty() ? m_pythonPackagesDir : m_pythonPackagesDir + QDir::listSeparator() + existingPythonPath);
    process->setProcessEnvironment(environment);
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, process, extracted](int exitCode, QProcess::ExitStatus exitStatus) {
                m_modelDownloadProcess = nullptr;
                process->deleteLater();
                if (m_modelDownloadCancellationRequested)
                {
                    QFile::remove(extracted);
                    finishModelDownload(QStringLiteral("模型下载已停止"));
                    return;
                }
                if (exitStatus != QProcess::NormalExit || exitCode != 0 ||
                    !fileMatchesSha256(extracted, QByteArrayLiteral("3a62f866d7139b45f061e7cd9eca5bb7242a1d18ada822b7e67fc0cba638ea53")))
                {
                    QFile::remove(extracted);
                    finishModelDownload(QStringLiteral("模型提取失败：FashionCLIP 图像模型无效"));
                    return;
                }
                const QString target = QDir(modelDirectory()).absoluteFilePath(QStringLiteral("fashion_clip_vision.onnx"));
                QFile::remove(target);
                if (!QFile::copy(extracted, target))
                {
                    finishModelDownload(QStringLiteral("模型提取失败：无法保存 FashionCLIP 图像模型"));
                    return;
                }
                finishModelDownload(QStringLiteral("模型下载完成：%1").arg(modelDirectory()));
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            m_modelDownloadProcess = nullptr;
            const QString message  = process->errorString();
            process->deleteLater();
            finishModelDownload(QStringLiteral("模型提取失败：%1").arg(message));
        }
    });
    process->start(m_pythonExecutable,
                   {QStringLiteral("-c"),
                    QStringLiteral("import sys; from onnx.utils import extract_model; "
                                   "extract_model(sys.argv[1], sys.argv[2], ['pixel_values'], ['image_embeds'], check_model=True)"),
                    source,
                    extracted});
}

void MatchController::connectModelProcessOutput(QProcess *process)
{
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        const QString message = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        if (!message.isEmpty())
        {
            emit logMessage(message);
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        const QString message = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        if (!message.isEmpty())
        {
            emit logMessage(message);
        }
    });
}

void MatchController::finishModelDownload(const QString &message)
{
    if (!m_modelDownloadInProgress)
    {
        return;
    }
    m_modelDownloadInProgress            = false;
    m_modelDownloadCancellationRequested = false;
    emit modelDownloadInProgressChanged();
    emit modelsAvailableChanged();
    emit logMessage(message);
}

void MatchController::openModelDirectory()
{
    QDir().mkpath(modelDirectory());
    QDesktopServices::openUrl(QUrl::fromLocalFile(modelDirectory()));
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

QString MatchController::previousPhotoPath() const
{
    if (!m_photoModel)
    {
        return {};
    }
    const auto *photo = m_photoModel->at(m_currentPhotoIndex - 1);
    return photo ? photo->imagePath : QString();
}

QString MatchController::nextPhotoPath() const
{
    if (!m_photoModel)
    {
        return {};
    }
    const auto *photo = m_photoModel->at(m_currentPhotoIndex + 1);
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
    restoreAutoMatchResult();
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
    restoreAutoMatchResult();
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
    restoreAutoMatchResult();
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

    clearAutoMatchResult();
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
    refreshPhotoMatchStatuses();
    const int  nextPhotoIndex   = m_photoModel->rowCount() > 0 ? 0 : -1;
    const bool selectionChanges = nextPhotoIndex != m_currentPhotoIndex || m_previewSource != PreviewPhoto;
    setCurrentPhotoIndex(nextPhotoIndex);
    emit currentPhotoPathChanged();
    if (!selectionChanges)
    {
        restoreAutoMatchResult();
    }
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
    const int  nextIndex        = m_candidateModel->rowCount() > 0 ? 0 : -1;
    const bool selectionChanges = nextIndex != m_currentIndex || m_previewSource != PreviewOutput;
    setCurrentIndex(nextIndex);
    if (!selectionChanges)
    {
        emitCurrentChanged();
        restoreAutoMatchResult();
    }
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

void MatchController::setBatchAutoMatchInProgress(bool inProgress)
{
    if (m_batchAutoMatchInProgress == inProgress)
    {
        return;
    }
    m_batchAutoMatchInProgress = inProgress;
    emit batchAutoMatchInProgressChanged();
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
    restoreAutoMatchResult();
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
    restoreAutoMatchResult();
}

void MatchController::previousUnmatchedPhoto()
{
    navigatePhoto(-1, PhotoNavigationFilter::Unmatched);
}

void MatchController::nextUnmatchedPhoto()
{
    navigatePhoto(1, PhotoNavigationFilter::Unmatched);
}

void MatchController::previousUnconfirmedPhoto()
{
    navigatePhoto(-1, PhotoNavigationFilter::Unconfirmed);
}

void MatchController::nextUnconfirmedPhoto()
{
    navigatePhoto(1, PhotoNavigationFilter::Unconfirmed);
}

void MatchController::navigatePhoto(int direction, PhotoNavigationFilter filter)
{
    if (m_previewSource != PreviewPhoto || !m_photoModel || m_currentPhotoIndex < 0)
    {
        return;
    }

    for (int row = m_currentPhotoIndex + direction; row >= 0 && row < m_photoModel->rowCount(); row += direction)
    {
        const PhotoItem *photo = m_photoModel->at(row);
        if (!photo)
        {
            continue;
        }

        const bool hasUnmatched   = photo->upperMatchStatus == PhotoMatchStatus::Unmatched || photo->lowerMatchStatus == PhotoMatchStatus::Unmatched;
        const bool hasUnconfirmed = photo->upperMatchStatus != PhotoMatchStatus::Confirmed || photo->lowerMatchStatus != PhotoMatchStatus::Confirmed;
        const bool matches        = filter == PhotoNavigationFilter::Unmatched ? hasUnmatched : hasUnconfirmed;
        if (matches)
        {
            setCurrentPhotoIndex(row);
            return;
        }
    }
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

// QFutureWatcher is parent-owned immediately and also scheduled for deletion when its future finishes.
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void MatchController::autoMatchStyleIds()
{
    if (m_busy)
    {
        emit logMessage(QStringLiteral("已有后台任务正在运行，请等待完成"));
        return;
    }
    const QString imagePath = currentImagePath();
    if (imagePath.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：当前没有可匹配的图片"));
        return;
    }
    if (m_photoDir.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：请先选择实拍图输入目录"));
        return;
    }

    QString    loadError;
    const auto storedResult = MatchResultStore::load(matchDatabasePath(), imagePath, &loadError);
    if (!loadError.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：读取当前图片款号记录时出错：%1").arg(loadError));
        return;
    }
    const StoredMatchResult existingResult = storedResult.value_or(StoredMatchResult {});
    if (existingResult.allMatchesConfirmed())
    {
        emit logMessage(QStringLiteral("当前实拍图的上衣和裤裙款号均已确认，已跳过自动匹配"));
        return;
    }
    if (!m_galleryModel)
    {
        emit logMessage(QStringLiteral("自动匹配失败：款号小图库模型不可用"));
        return;
    }
    const QVector<GalleryItem> galleryItems = m_galleryModel->allItems();
    if (galleryItems.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：请先从 PPT 提取款号小图库"));
        return;
    }

    const QString modelsDir = availableModelDirectory();
    if (modelsDir.isEmpty())
    {
        emit logMessage(QStringLiteral("自动匹配失败：未找到可用模型，请从顶部“下载模型”菜单下载"));
        emit modelDownloadRequired();
        return;
    }

    const GarmentMatcher::Options options = matcherOptions(modelsDir);

    QElapsedTimer matchTimer;
    matchTimer.start();
    auto *watcher = new QFutureWatcher<GarmentMatcher::Result>(this);
    connect(watcher, &QFutureWatcher<GarmentMatcher::Result>::finished, this, [this, watcher, imagePath, matchTimer] {
        const GarmentMatcher::Result result = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        if (currentImagePath() != imagePath)
        {
            emit logMessage(QStringLiteral("自动匹配已完成，但当前图片已切换，结果未回填"));
            return;
        }
        if (!result.success)
        {
            emit logMessage(QStringLiteral("自动匹配失败：%1").arg(result.error));
            return;
        }

        QString    loadError;
        const auto latestStoredResult = MatchResultStore::load(matchDatabasePath(), imagePath, &loadError);
        if (!loadError.isEmpty())
        {
            emit logMessage(QStringLiteral("自动匹配失败：保存前读取当前图片款号记录时出错：%1").arg(loadError));
            return;
        }
        m_autoMatchResult = latestStoredResult.value_or(StoredMatchResult {});
        m_autoMatchResult.replaceUnconfirmedMatches(storedMatchResult(result));
        m_autoMatchImagePath = imagePath;
        rebuildAutoMatchedItems();

        const auto matchSummary = [](const StoredGarmentMatch &stored, const GarmentMatcher::Match &matched) {
            if (stored.confirmed)
            {
                return QStringLiteral("%1（已确认，跳过）").arg(stored.styleId);
            }
            return QStringLiteral("%1 (%2)").arg(matched.styleId.isEmpty() ? QStringLiteral("未检出") : matched.styleId,
                                                 QString::number(matched.score, 'f', 3));
        };
        QString message =
            QStringLiteral("自动匹配完成（%1）：上衣 %2，下装 %3，耗时 %4 毫秒")
                .arg(result.provider, matchSummary(m_autoMatchResult.upper, result.upper), matchSummary(m_autoMatchResult.lower, result.lower))
                .arg(matchTimer.elapsed());
        QString persistenceError;
        if (!persistAutoMatchResult(&persistenceError))
        {
            message += QStringLiteral("；未写入 gsm.db：%1").arg(persistenceError);
        }
        else
        {
            updatePhotoMatchStatuses(imagePath, m_autoMatchResult);
        }
        emit logMessage(message);
    });

    setBusy(true);
    emit logMessage(QStringLiteral("正在分割服装并匹配 %1 张款号图片...").arg(galleryItems.size()));
    watcher->setFuture(QtConcurrent::run([imagePath, galleryItems, options] { return GarmentMatcher::match(imagePath, galleryItems, options); }));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void MatchController::autoMatchAllStyleIds() // NOLINT(readability-function-cognitive-complexity)
{
    if (m_busy)
    {
        emit logMessage(QStringLiteral("已有后台任务正在运行，请等待完成"));
        return;
    }
    if (!m_photoModel || m_photoModel->allItems().isEmpty())
    {
        emit logMessage(QStringLiteral("批量自动匹配失败：输入实拍图列表为空"));
        return;
    }
    if (m_photoDir.isEmpty())
    {
        emit logMessage(QStringLiteral("批量自动匹配失败：请先选择实拍图输入目录"));
        return;
    }

    QStringList photoPaths;
    photoPaths.reserve(m_photoModel->allItems().size());
    for (const PhotoItem &photo : m_photoModel->allItems())
    {
        photoPaths.push_back(photo.imagePath);
    }

    const QVector<GalleryItem>    galleryItems = m_galleryModel ? m_galleryModel->allItems() : QVector<GalleryItem> {};
    const QString                 modelsDir    = availableModelDirectory();
    const GarmentMatcher::Options options      = matcherOptions(modelsDir);
    const QString                 databasePath = matchDatabasePath();
    QElapsedTimer                 matchTimer;
    matchTimer.start();
    const auto cancellation      = std::make_shared<std::atomic_bool>(false);
    m_batchAutoMatchCancellation = cancellation;
    const int totalPhotos        = static_cast<int>(photoPaths.size());
    auto     *watcher            = new QFutureWatcher<BatchAutoMatchSummary>(this);
    connect(watcher, &QFutureWatcher<BatchAutoMatchSummary>::finished, this, [this, watcher, matchTimer, cancellation, totalPhotos] {
        BatchAutoMatchSummary summary = watcher->result();
        if (cancellation->load() && !summary.cancelled)
        {
            summary.cancelled   = true;
            summary.unprocessed = totalPhotos - summary.succeeded - summary.skipped - summary.failed;
        }
        watcher->deleteLater();
        m_batchAutoMatchCancellation.reset();
        setBatchAutoMatchInProgress(false);
        setBusy(false);
        refreshPhotoMatchStatuses();
        if (m_previewSource == PreviewPhoto)
        {
            restoreAutoMatchResult();
        }
        if (summary.modelDownloadRequired)
        {
            emit modelDownloadRequired();
        }

        QString message;
        if (summary.cancelled)
        {
            message = QStringLiteral("批量自动匹配已停止：成功 %1 张，跳过 %2 张，失败 %3 张，未处理 %4 张，耗时 %5 毫秒")
                          .arg(summary.succeeded)
                          .arg(summary.skipped)
                          .arg(summary.failed)
                          .arg(summary.unprocessed)
                          .arg(matchTimer.elapsed());
        }
        else
        {
            message = QStringLiteral("批量自动匹配完成：成功 %1 张，跳过 %2 张，失败 %3 张，耗时 %4 毫秒")
                          .arg(summary.succeeded)
                          .arg(summary.skipped)
                          .arg(summary.failed)
                          .arg(matchTimer.elapsed());
        }
        if (!summary.firstError.isEmpty())
        {
            message += QStringLiteral("；首个错误：%1").arg(summary.firstError);
        }
        emit logMessage(message);
    });

    setBatchAutoMatchInProgress(true);
    setBusy(true);
    emit      logMessage(QStringLiteral("正在检查并使用 %1 个线程批量匹配 %2 张实拍图...").arg(m_parallelMatchThreadCount).arg(photoPaths.size()));
    const int parallelThreadCount = m_parallelMatchThreadCount;
    // The worker intentionally keeps cancellation, preflight, matching, merging, and summary accounting in one sequential transaction.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    watcher->setFuture(QtConcurrent::run([photoPaths, galleryItems, modelsDir, options, databasePath, cancellation, parallelThreadCount] {
        BatchAutoMatchSummary summary;
        const auto            cancelledSummary = [&] {
            summary.cancelled   = true;
            summary.unprocessed = static_cast<int>(photoPaths.size()) - summary.succeeded - summary.skipped - summary.failed;
            return summary;
        };
        QStringList pendingPhotoPaths;
        pendingPhotoPaths.reserve(photoPaths.size());
        for (const QString &photoPath : photoPaths)
        {
            if (cancellation->load())
            {
                return cancelledSummary();
            }
            QString    error;
            const auto existingResult = MatchResultStore::load(databasePath, photoPath, &error);
            if (!error.isEmpty())
            {
                ++summary.failed;
                if (summary.firstError.isEmpty())
                {
                    summary.firstError = QStringLiteral("%1：%2").arg(QFileInfo(photoPath).fileName(), error);
                }
                continue;
            }
            if (existingResult && existingResult->allMatchesConfirmed())
            {
                ++summary.skipped;
                continue;
            }
            pendingPhotoPaths.push_back(photoPath);
        }

        if (cancellation->load())
        {
            return cancelledSummary();
        }
        if (pendingPhotoPaths.isEmpty())
        {
            return summary;
        }
        if (galleryItems.isEmpty())
        {
            summary.failed += static_cast<int>(pendingPhotoPaths.size());
            if (summary.firstError.isEmpty())
            {
                summary.firstError = QStringLiteral("请先从 PPT 提取款号小图库");
            }
            return summary;
        }
        if (modelsDir.isEmpty())
        {
            summary.failed += static_cast<int>(pendingPhotoPaths.size());
            summary.modelDownloadRequired = true;
            if (summary.firstError.isEmpty())
            {
                summary.firstError = QStringLiteral("未找到可用模型，请从顶部“下载模型”菜单下载");
            }
            return summary;
        }

        const QVector<GarmentMatcher::Result> results =
            GarmentMatcher::matchAll(pendingPhotoPaths, galleryItems, options, cancellation.get(), parallelThreadCount);
        const bool matchingCancelled = results.size() < pendingPhotoPaths.size();
        for (qsizetype index = 0; index < results.size(); ++index)
        {
            if (!matchingCancelled && cancellation->load())
            {
                return cancelledSummary();
            }
            const QString                &photoPath = pendingPhotoPaths.at(index);
            const GarmentMatcher::Result &result    = results.at(index);
            if (!result.success)
            {
                ++summary.failed;
                if (summary.firstError.isEmpty())
                {
                    summary.firstError = QStringLiteral("%1：%2").arg(QFileInfo(photoPath).fileName(), result.error);
                }
                continue;
            }

            QString    error;
            const auto latestStoredResult = MatchResultStore::load(databasePath, photoPath, &error);
            if (!error.isEmpty())
            {
                ++summary.failed;
                if (summary.firstError.isEmpty())
                {
                    summary.firstError = QStringLiteral("%1：%2").arg(QFileInfo(photoPath).fileName(), error);
                }
                continue;
            }
            StoredMatchResult mergedResult = latestStoredResult.value_or(StoredMatchResult {});
            if (mergedResult.allMatchesConfirmed())
            {
                ++summary.skipped;
                continue;
            }
            mergedResult.replaceUnconfirmedMatches(storedMatchResult(result));
            if (MatchResultStore::save(databasePath, photoPath, mergedResult, &error))
            {
                ++summary.succeeded;
                continue;
            }
            ++summary.failed;
            if (summary.firstError.isEmpty())
            {
                summary.firstError = QStringLiteral("%1：%2").arg(QFileInfo(photoPath).fileName(), error);
            }
        }
        if (matchingCancelled)
        {
            return cancelledSummary();
        }
        return summary;
    }));
}

void MatchController::cancelAutoMatchAllStyleIds()
{
    if (!m_batchAutoMatchInProgress || !m_batchAutoMatchCancellation)
    {
        return;
    }
    if (!m_batchAutoMatchCancellation->exchange(true))
    {
        emit logMessage(QStringLiteral("正在停止自动匹配，当前图片处理完成后停止..."));
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool MatchController::copyWouldOverwriteConfirmedStyleIds(int offset, const QString &part, bool targetAdjacent) const
{
    if (m_previewSource != PreviewPhoto || !m_photoModel || m_currentPhotoIndex < 0 || (offset != -1 && offset != 1))
    {
        return false;
    }

    if (!targetAdjacent)
    {
        const PhotoItem *sourcePhoto = m_photoModel->at(m_currentPhotoIndex + offset);
        if (!sourcePhoto)
        {
            return false;
        }
        QString    error;
        const auto sourceResult = MatchResultStore::load(matchDatabasePath(), sourcePhoto->imagePath, &error);
        if (!sourceResult || !error.isEmpty())
        {
            return false;
        }
        const bool hasSource = part == QLatin1String("all")     ? !sourceResult->isEmpty()
                               : part == QLatin1String("upper") ? !sourceResult->upper.isEmpty()
                               : part == QLatin1String("lower") ? !sourceResult->lower.isEmpty()
                                                                : false;
        return hasSource && m_autoMatchImagePath == currentPhotoPath() && hasConfirmedMatch(m_autoMatchResult, part);
    }

    const PhotoItem *targetPhoto = m_photoModel->at(m_currentPhotoIndex + offset);
    if (!targetPhoto || m_autoMatchImagePath != currentPhotoPath())
    {
        return false;
    }
    const bool hasSource = part == QLatin1String("all")     ? !m_autoMatchResult.isEmpty()
                           : part == QLatin1String("upper") ? !m_autoMatchResult.upper.isEmpty()
                           : part == QLatin1String("lower") ? !m_autoMatchResult.lower.isEmpty()
                                                            : false;
    if (!hasSource)
    {
        return false;
    }
    QString    error;
    const auto targetResult = MatchResultStore::load(matchDatabasePath(), targetPhoto->imagePath, &error);
    return targetResult && error.isEmpty() && hasConfirmedMatch(*targetResult, part);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool MatchController::copyAdjacentStyleIds(int offset, const QString &part, bool overwriteConfirmed)
{
    if (m_previewSource != PreviewPhoto || !m_photoModel || m_currentPhotoIndex < 0)
    {
        emit logMessage(QStringLiteral("复制款号失败：请先选择一张实拍图"));
        return false;
    }
    if ((offset != -1 && offset != 1) || (part != QLatin1String("all") && part != QLatin1String("upper") && part != QLatin1String("lower")))
    {
        emit logMessage(QStringLiteral("复制款号失败：参数无效"));
        return false;
    }

    const int        sourceIndex = m_currentPhotoIndex + offset;
    const PhotoItem *sourcePhoto = m_photoModel->at(sourceIndex);
    if (!sourcePhoto)
    {
        emit logMessage(offset < 0 ? QStringLiteral("复制款号失败：当前已是第一张实拍图") : QStringLiteral("复制款号失败：当前已是最后一张实拍图"));
        return false;
    }

    QString    error;
    const auto sourceResult = MatchResultStore::load(matchDatabasePath(), sourcePhoto->imagePath, &error);
    if (!error.isEmpty())
    {
        emit logMessage(QStringLiteral("复制款号失败：读取相邻图片记录时出错：%1").arg(error));
        return false;
    }
    if (!sourceResult)
    {
        emit logMessage(QStringLiteral("复制款号失败：相邻图片没有款号记录"));
        return false;
    }

    StoredMatchResult copied      = m_autoMatchResult;
    const auto        unconfirmed = [](StoredGarmentMatch match) {
        match.confirmed = false;
        return match;
    };
    if (part == QLatin1String("all"))
    {
        copied.upper = unconfirmed(sourceResult->upper);
        copied.lower = unconfirmed(sourceResult->lower);
    }
    else
    {
        const StoredGarmentMatch &sourceMatch = part == QLatin1String("upper") ? sourceResult->upper : sourceResult->lower;
        if (sourceMatch.isEmpty())
        {
            emit logMessage(QStringLiteral("复制款号失败：相邻图片没有%1款号")
                                .arg(part == QLatin1String("upper") ? QStringLiteral("上衣") : QStringLiteral("裤裙")));
            return false;
        }
        (part == QLatin1String("upper") ? copied.upper : copied.lower) = unconfirmed(sourceMatch);
    }

    if (!overwriteConfirmed && m_autoMatchImagePath == currentPhotoPath() && hasConfirmedMatch(m_autoMatchResult, part))
    {
        emit logMessage(QStringLiteral("复制款号已取消：目标实拍图已有被确认的款号"));
        return false;
    }

    const QString targetImagePath = currentPhotoPath();
    if (!MatchResultStore::save(matchDatabasePath(), targetImagePath, copied, &error))
    {
        emit logMessage(QStringLiteral("复制款号失败：%1").arg(error));
        return false;
    }

    m_autoMatchResult    = copied;
    m_autoMatchImagePath = targetImagePath;
    rebuildAutoMatchedItems();
    updatePhotoMatchStatuses(targetImagePath, copied);
    const QString direction = offset < 0 ? QStringLiteral("上一张") : QStringLiteral("下一张");
    const QString garment   = part == QLatin1String("all")     ? QString()
                              : part == QLatin1String("upper") ? QStringLiteral("上衣")
                                                               : QStringLiteral("裤裙");
    emit          logMessage(QStringLiteral("已复制%1%2款号").arg(direction, garment));
    return true;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool MatchController::copyStyleIdsToAdjacent(int offset, const QString &part, bool overwriteConfirmed)
{
    if (m_previewSource != PreviewPhoto || !m_photoModel || m_currentPhotoIndex < 0 || m_autoMatchImagePath != currentPhotoPath())
    {
        emit logMessage(QStringLiteral("复制款号失败：当前实拍图没有款号记录"));
        return false;
    }
    if ((offset != -1 && offset != 1) || (part != QLatin1String("all") && part != QLatin1String("upper") && part != QLatin1String("lower")))
    {
        emit logMessage(QStringLiteral("复制款号失败：参数无效"));
        return false;
    }

    const int        targetIndex = m_currentPhotoIndex + offset;
    const PhotoItem *targetPhoto = m_photoModel->at(targetIndex);
    if (!targetPhoto)
    {
        emit logMessage(offset < 0 ? QStringLiteral("复制款号失败：当前已是第一张实拍图") : QStringLiteral("复制款号失败：当前已是最后一张实拍图"));
        return false;
    }

    const StoredGarmentMatch &sourceMatch = part == QLatin1String("upper") ? m_autoMatchResult.upper : m_autoMatchResult.lower;
    if ((part == QLatin1String("all") && m_autoMatchResult.isEmpty()) || (part != QLatin1String("all") && sourceMatch.isEmpty()))
    {
        const QString garment = part == QLatin1String("upper")   ? QStringLiteral("上衣")
                                : part == QLatin1String("lower") ? QStringLiteral("裤裙")
                                                                 : QString();
        emit          logMessage(QStringLiteral("复制款号失败：当前实拍图没有%1款号").arg(garment));
        return false;
    }

    QString           error;
    StoredMatchResult targetResult;
    const auto        existingResult = MatchResultStore::load(matchDatabasePath(), targetPhoto->imagePath, &error);
    if (!error.isEmpty())
    {
        emit logMessage(QStringLiteral("复制款号失败：读取目标图片记录时出错：%1").arg(error));
        return false;
    }
    if (existingResult)
    {
        targetResult = *existingResult;
    }

    if (!overwriteConfirmed && hasConfirmedMatch(targetResult, part))
    {
        emit logMessage(QStringLiteral("复制款号已取消：目标实拍图已有被确认的款号"));
        return false;
    }

    const auto unconfirmed = [](StoredGarmentMatch match) {
        match.confirmed = false;
        return match;
    };
    if (part == QLatin1String("all"))
    {
        targetResult       = {};
        targetResult.upper = unconfirmed(m_autoMatchResult.upper);
        targetResult.lower = unconfirmed(m_autoMatchResult.lower);
    }
    else
    {
        (part == QLatin1String("upper") ? targetResult.upper : targetResult.lower) = unconfirmed(sourceMatch);
    }

    if (!MatchResultStore::save(matchDatabasePath(), targetPhoto->imagePath, targetResult, &error))
    {
        emit logMessage(QStringLiteral("复制款号失败：%1").arg(error));
        return false;
    }
    updatePhotoMatchStatuses(targetPhoto->imagePath, targetResult);

    const QString direction = offset < 0 ? QStringLiteral("上一张") : QStringLiteral("下一张");
    const QString garment   = part == QLatin1String("all")     ? QString()
                              : part == QLatin1String("upper") ? QStringLiteral("上衣")
                                                               : QStringLiteral("裤裙");
    emit          logMessage(QStringLiteral("已将%1款号复制到%2").arg(garment, direction));
    return true;
}

void MatchController::clearAutoMatchResult()
{
    const bool hadItems = !m_autoMatchedItems.isEmpty();
    m_autoMatchResult   = {};
    m_autoMatchImagePath.clear();
    m_autoMatchedItems.clear();
    if (hadItems)
    {
        emit autoMatchedItemsChanged();
    }
}

void MatchController::restoreAutoMatchResult()
{
    clearAutoMatchResult();
    const QString imagePath = currentImagePath();
    if (imagePath.isEmpty() || m_photoDir.isEmpty())
    {
        return;
    }

    QString    error;
    const auto result = MatchResultStore::load(matchDatabasePath(), imagePath, &error);
    if (!error.isEmpty())
    {
        emit logMessage(QStringLiteral("读取 gsm.db 失败：%1").arg(error));
        return;
    }
    if (!result)
    {
        updatePhotoMatchStatuses(imagePath, {});
        return;
    }

    m_autoMatchResult    = *result;
    m_autoMatchImagePath = imagePath;
    rebuildAutoMatchedItems();
    updatePhotoMatchStatuses(imagePath, *result);
}

void MatchController::rebuildAutoMatchedItems()
{
    QVariantList items;
    const auto   append = [this, &items](const StoredGarmentMatch &match, const QString &part, const QString &garment) {
        if (match.isEmpty())
        {
            return;
        }
        QVariantMap item;
        item.insert(QStringLiteral("part"), part);
        item.insert(QStringLiteral("garment"), garment);
        item.insert(QStringLiteral("styleId"), match.styleId);
        item.insert(QStringLiteral("imagePath"), galleryImagePath(match));
        item.insert(QStringLiteral("confirmed"), match.confirmed);
        items.push_back(item);
    };
    append(m_autoMatchResult.upper, QStringLiteral("upper"), QStringLiteral("上衣"));
    append(m_autoMatchResult.lower, QStringLiteral("lower"), QStringLiteral("裤裙"));
    if (items == m_autoMatchedItems)
    {
        return;
    }
    m_autoMatchedItems = items;
    emit autoMatchedItemsChanged();
}

void MatchController::refreshPhotoMatchStatuses()
{
    if (!m_photoModel || m_photoDir.isEmpty())
    {
        return;
    }

    const QVector<PhotoItem> photos = m_photoModel->allItems();
    for (const PhotoItem &photo : photos)
    {
        QString    error;
        const auto result = MatchResultStore::load(matchDatabasePath(), photo.imagePath, &error);
        if (!error.isEmpty())
        {
            emit logMessage(QStringLiteral("读取 %1 的款号状态失败：%2").arg(photo.fileName, error));
            continue;
        }
        updatePhotoMatchStatuses(photo.imagePath, result.value_or(StoredMatchResult {}));
    }
}

void MatchController::updatePhotoMatchStatuses(const QString &imagePath, const StoredMatchResult &result)
{
    if (!m_photoModel)
    {
        return;
    }
    m_photoModel->setMatchStatuses(imagePath, photoMatchStatus(result.upper), photoMatchStatus(result.lower));
}

bool MatchController::persistAutoMatchResult(QString *error) const
{
    if (m_photoDir.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral("请先选择实拍图输入目录");
        }
        return false;
    }
    return MatchResultStore::save(matchDatabasePath(), m_autoMatchImagePath, m_autoMatchResult, error);
}

QString MatchController::matchDatabasePath() const
{
    return QDir(m_photoDir).absoluteFilePath(QStringLiteral("gsm.db"));
}

QString MatchController::galleryImagePath(const StoredGarmentMatch &match) const
{
    if (!m_galleryModel)
    {
        return {};
    }
    for (const GalleryItem &item : m_galleryModel->allItems())
    {
        if (item.styleId == match.styleId && QFileInfo(item.imagePath).fileName().compare(match.imageName, Qt::CaseInsensitive) == 0)
        {
            return item.imagePath;
        }
    }
    return {};
}

void MatchController::confirmAutoMatch(const QString &part)
{
    if (m_autoMatchImagePath != currentImagePath())
    {
        return;
    }
    StoredGarmentMatch *match = part == QLatin1String("upper")   ? &m_autoMatchResult.upper
                                : part == QLatin1String("lower") ? &m_autoMatchResult.lower
                                                                 : nullptr;
    if (!match || match->isEmpty())
    {
        return;
    }

    match->confirmed = true;
    rebuildAutoMatchedItems();
    QString error;
    if (!persistAutoMatchResult(&error))
    {
        emit logMessage(QStringLiteral("确认结果未写入 gsm.db：%1").arg(error));
        return;
    }
    updatePhotoMatchStatuses(m_autoMatchImagePath, m_autoMatchResult);
    emit logMessage(
        QStringLiteral("%1款号 %2 已确认").arg(part == QLatin1String("upper") ? QStringLiteral("上衣") : QStringLiteral("裤裙"), match->styleId));
}

void MatchController::rejectAutoMatch(const QString &part)
{
    if (m_autoMatchImagePath != currentImagePath())
    {
        return;
    }
    StoredGarmentMatch *match = part == QLatin1String("upper")   ? &m_autoMatchResult.upper
                                : part == QLatin1String("lower") ? &m_autoMatchResult.lower
                                                                 : nullptr;
    if (!match || match->isEmpty())
    {
        return;
    }

    const QString garment = part == QLatin1String("upper") ? QStringLiteral("上衣") : QStringLiteral("裤裙");
    *match                = {};
    rebuildAutoMatchedItems();
    QString error;
    if (!persistAutoMatchResult(&error))
    {
        emit logMessage(QStringLiteral("删除%1匹配后未能更新 gsm.db：%2").arg(garment, error));
        return;
    }
    updatePhotoMatchStatuses(m_autoMatchImagePath, m_autoMatchResult);
    emit logMessage(QStringLiteral("已删除错误的%1匹配记录").arg(garment));
}

void MatchController::generateFineTuneModel()
{
    emit logMessage(QStringLiteral("generateFineTuneModel triggered"));
}
// NOLINTEND(readability-identifier-length)
