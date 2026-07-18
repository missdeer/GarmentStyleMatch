#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "MatchController.h"
#include "CandidateListModel.h"
#include "PhotoListModel.h"

namespace
{
    constexpr int kAsyncTimeoutMs             = 5000;
    constexpr int kInvalidParallelThreadCount = 9;

    bool check(bool condition, const QString &message)
    {
        if (condition)
        {
            return true;
        }
        std::cerr << message.toStdString() << '\n';
        return false;
    }

    bool createFile(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
    }

} // namespace

int main(int argc, char *argv[]) // NOLINT(readability-function-cognitive-complexity)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temporary;
    QTemporaryDir settingsTemporary;
    QTemporaryDir modelTemporary;
    if (!check(temporary.isValid() && settingsTemporary.isValid() && modelTemporary.isValid(), QStringLiteral("无法创建临时目录")))
    {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("EidosTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MatchControllerTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsTemporary.path());
    QSettings().clear();

    QDir root(temporary.path());
    if (!check(root.mkpath(QStringLiteral("Alpha/nested")) && root.mkpath(QStringLiteral("Beta")), QStringLiteral("无法创建测试子目录")))
    {
        return 1;
    }
    if (!check(createFile(root.filePath(QStringLiteral("Alpha/01.jpg"))) && createFile(root.filePath(QStringLiteral("Alpha/02.PNG"))) &&
                   createFile(root.filePath(QStringLiteral("Alpha/readme.txt"))) &&
                   createFile(root.filePath(QStringLiteral("Alpha/nested/03.jpg"))) && createFile(root.filePath(QStringLiteral("root.jpg"))),
               QStringLiteral("无法创建测试文件")))
    {
        return 1;
    }

    CandidateListModel model;
    MatchController    controller;
    controller.setCandidateModel(&model);

    QEventLoop deferredStartupLoop;
    QObject::connect(&controller, &MatchController::deferredStartupCompleted, &deferredStartupLoop, &QEventLoop::quit);
    QTimer::singleShot(kAsyncTimeoutMs, &deferredStartupLoop, &QEventLoop::quit);
    controller.completeDeferredStartup();
    deferredStartupLoop.exec();

    controller.setOutputDir(temporary.path());

    const QString expectedModelDirectory =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).absoluteFilePath(QStringLiteral("models"));
    if (!check(MatchController::modelDirectory() == expectedModelDirectory, QStringLiteral("下载模型必须保存到应用的 LocalAppData/models 目录")))
    {
        return 1;
    }

#ifdef Q_OS_MACOS
    const QString expectedApplicationModelDirectory =
        QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../Resources/models")));
#else
    const QString expectedApplicationModelDirectory = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("models"));
#endif
    if (!check(MatchController::applicationModelDirectory() == expectedApplicationModelDirectory,
               QStringLiteral("随应用打包的模型目录必须符合当前平台的应用布局")))
    {
        return 1;
    }

    QDir          modelRoot(modelTemporary.path());
    const QString applicationModelsDir = modelRoot.filePath(QStringLiteral("application-models"));
    const QString localModelsDir       = modelRoot.filePath(QStringLiteral("local-models"));
    if (!check(modelRoot.mkpath(QStringLiteral("application-models")) && modelRoot.mkpath(QStringLiteral("local-models")) &&
                   createFile(QDir(applicationModelsDir).filePath(QStringLiteral("readme.txt"))),
               QStringLiteral("无法创建模型文件检测测试目录")))
    {
        return 1;
    }
    if (!check(!MatchController::modelFilesExistInDirectories(applicationModelsDir, localModelsDir),
               QStringLiteral("模型目录中的无关文件不得触发重新下载确认")))
    {
        return 1;
    }
    const QString applicationSegmentationModel = QDir(applicationModelsDir).filePath(QStringLiteral("clothes_segformer_b2.onnx"));
    if (!check(createFile(applicationSegmentationModel) && MatchController::modelFilesExistInDirectories(applicationModelsDir, localModelsDir),
               QStringLiteral("applicationDir/models 中存在任一模型文件时必须触发重新下载确认")))
    {
        return 1;
    }
    QFile::remove(applicationSegmentationModel);
    const QString localEmbeddingModel = QDir(localModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx"));
    if (!check(createFile(localEmbeddingModel) && MatchController::modelFilesExistInDirectories(applicationModelsDir, localModelsDir),
               QStringLiteral("LocalAppData/models 中存在任一模型文件时必须触发重新下载确认")))
    {
        return 1;
    }
    QFile::remove(localEmbeddingModel);
    if (!check(modelRoot.mkpath(QStringLiteral("application-models")) && modelRoot.mkpath(QStringLiteral("local-models")) &&
                   createFile(QDir(applicationModelsDir).filePath(QStringLiteral("clothes_segformer_b2.onnx"))) &&
                   createFile(QDir(applicationModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx"))) &&
                   createFile(QDir(localModelsDir).filePath(QStringLiteral("clothes_segformer_b2.onnx"))) &&
                   createFile(QDir(localModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx"))),
               QStringLiteral("无法创建模型查找顺序测试文件")))
    {
        return 1;
    }
    if (!check(MatchController::findAvailableModelDirectory(applicationModelsDir, localModelsDir) == applicationModelsDir,
               QStringLiteral("匹配必须优先使用 applicationDir/models 中的完整模型")))
    {
        return 1;
    }
    QFile::remove(QDir(applicationModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx")));
    if (!check(MatchController::findAvailableModelDirectory(applicationModelsDir, localModelsDir) == localModelsDir,
               QStringLiteral("应用目录模型不完整时必须回退到 LocalAppData/models")))
    {
        return 1;
    }
    QFile::remove(QDir(localModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx")));
    if (!check(MatchController::findAvailableModelDirectory(applicationModelsDir, localModelsDir).isEmpty(),
               QStringLiteral("两个目录的模型都不完整时必须报告无可用模型")))
    {
        return 1;
    }

#ifdef Q_OS_WIN
    bool    modelDownloadStopped = false;
    QString lastModelDownloadMessage;
    QObject::connect(
        &controller, &MatchController::logMessage, &controller, [&modelDownloadStopped, &lastModelDownloadMessage](const QString &message) {
            lastModelDownloadMessage = message;
            if (message == QStringLiteral("模型下载已停止"))
            {
                modelDownloadStopped = true;
            }
        });
    QEventLoop downloadStopLoop;
    QObject::connect(&controller, &MatchController::modelDownloadInProgressChanged, &downloadStopLoop, [&] {
        if (!controller.modelDownloadInProgress())
        {
            downloadStopLoop.quit();
        }
    });
    controller.downloadModels();
    if (!check(controller.modelDownloadInProgress(), QStringLiteral("触发模型下载后必须进入下载状态，实际消息：%1").arg(lastModelDownloadMessage)))
    {
        return 1;
    }
    QTimer::singleShot(0, &controller, &MatchController::cancelModelDownload);
    QTimer::singleShot(kAsyncTimeoutMs, &downloadStopLoop, &QEventLoop::quit);
    downloadStopLoop.exec();
    if (!check(!controller.modelDownloadInProgress() && modelDownloadStopped, QStringLiteral("停止下载必须终止下载进程并报告已停止")))
    {
        return 1;
    }
#endif

    if (!check(controller.availableInferenceEngines().contains(QStringLiteral("CPU")), QStringLiteral("推理引擎列表必须始终包含 CPU")))
    {
        return 1;
    }
    const QString activeInferenceEngine = controller.currentInferenceEngine();
    if (!check(controller.parallelMatchThreadCount() == 1, QStringLiteral("所有推理引擎首次启动都应只创建一套 Runtime")))
    {
        return 1;
    }
    controller.setParallelMatchThreadCount(3);
    if (!check(controller.parallelMatchThreadCount() == 3 && QSettings().value(QStringLiteral("matching/parallelThreads")).toInt() == 3,
               QStringLiteral("并行匹配线程数必须持久化")))
    {
        return 1;
    }
    controller.setParallelMatchThreadCount(0);
    controller.setParallelMatchThreadCount(kInvalidParallelThreadCount);
    if (!check(controller.parallelMatchThreadCount() == 3, QStringLiteral("并行匹配线程数必须限制为 1 到 8")))
    {
        return 1;
    }
    MatchController restoredParallelThreadController;
    if (!check(restoredParallelThreadController.parallelMatchThreadCount() == 3, QStringLiteral("重建控制器后必须恢复并行匹配线程数")))
    {
        return 1;
    }
    if (!check(controller.setCurrentInferenceEngine(QStringLiteral("CPU")) || controller.currentInferenceEngine() == QStringLiteral("CPU"),
               QStringLiteral("应允许选择 CPU 推理引擎")))
    {
        return 1;
    }
    if (!check(QSettings().value(QStringLiteral("matching/provider")).toString() == QStringLiteral("cpu"),
               QStringLiteral("推理引擎选择应持久化到 matching/provider")))
    {
        return 1;
    }
#ifdef Q_OS_WIN
    if (!check(controller.availableInferenceEngines().contains(QStringLiteral("Windows ML · CPU")) &&
                   controller.availableInferenceEngines().contains(QStringLiteral("Windows ML · DirectML")),
               QStringLiteral("Windows 构建必须分别提供 Windows ML CPU 和 DirectML 推理引擎")) ||
        !check(controller.setCurrentInferenceEngine(QStringLiteral("Windows ML · CPU")) &&
                   QSettings().value(QStringLiteral("matching/provider")).toString() == QStringLiteral("windows ml · cpu"),
               QStringLiteral("Windows ML CPU 推理引擎必须可选并持久化")))
    {
        return 1;
    }
#endif
    if (!check(controller.currentInferenceEngine() == activeInferenceEngine, QStringLiteral("推理引擎设置在重启前不得改变当前进程使用的引擎")))
    {
        return 1;
    }
    if (!check(!controller.setCurrentInferenceEngine(QStringLiteral("unsupported")), QStringLiteral("不应接受本机不支持的推理引擎")))
    {
        return 1;
    }

    if (!check(QSettings().value(QStringLiteral("output/lastDir")).toString() == temporary.path(), QStringLiteral("输出目录应持久化保存")))
    {
        return 1;
    }

    if (!check(model.rowCount() == 2, QStringLiteral("输出列表应只包含两个直接子目录")))
    {
        return 1;
    }
    const QString alphaDisplay = model.data(model.index(0), CandidateListModel::DisplayLineRole).toString();
    if (!check(alphaDisplay == QStringLiteral("Alpha(2)"), QStringLiteral("Alpha 条目应显示直接包含的两张图片，实际为: %1").arg(alphaDisplay)))
    {
        return 1;
    }
    if (!check(model.data(model.index(1), CandidateListModel::DisplayLineRole).toString() == QStringLiteral("Beta(0)"),
               QStringLiteral("空子目录也应显示为一个条目")))
    {
        return 1;
    }
    if (!check(model.data(model.index(0), CandidateListModel::ImagePathRole).toString() == root.filePath(QStringLiteral("Alpha/01.jpg")),
               QStringLiteral("条目预览应使用子目录中的第一张图片")))
    {
        return 1;
    }
    if (!check(controller.currentOutputImagePaths().size() == 2, QStringLiteral("控制器应暴露选中目录中的全部图片")))
    {
        return 1;
    }
    controller.setOutputFilterText(QStringLiteral("beta"));
    if (!check(model.rowCount() == 1 && model.at(0)->styleId == QStringLiteral("Beta") && controller.currentIndex() == 0,
               QStringLiteral("输出列表应即时按款号进行不区分大小写的关键字过滤")))
    {
        return 1;
    }
    controller.setOutputFilterText(QString());
    if (!check(model.rowCount() == 2, QStringLiteral("清空输出过滤关键字后应恢复全部归类项")))
    {
        return 1;
    }
    controller.nextImage();
    if (!check(controller.currentImagePage() == 1 && controller.currentImagePath() == root.filePath(QStringLiteral("Alpha/02.PNG")),
               QStringLiteral("下一张应同步切换页索引和主图路径")))
    {
        return 1;
    }
    controller.previousImage();
    if (!check(controller.currentImagePage() == 0 && controller.currentImagePath() == root.filePath(QStringLiteral("Alpha/01.jpg")),
               QStringLiteral("上一张应同步切回前一个缩略图")))
    {
        return 1;
    }

    PhotoListModel     navigationPhotoModel;
    QVector<PhotoItem> navigationPhotos = {
        {QStringLiteral("unmatched-upper.jpg"),
         QStringLiteral("unmatched-upper.jpg"),
         false,
         {},
         PhotoMatchStatus::Unmatched,
         PhotoMatchStatus::Confirmed},
        {QStringLiteral("confirmed-1.jpg"), QStringLiteral("confirmed-1.jpg"), false, {}, PhotoMatchStatus::Confirmed, PhotoMatchStatus::Confirmed},
        {QStringLiteral("unconfirmed-upper.jpg"),
         QStringLiteral("unconfirmed-upper.jpg"),
         false,
         {},
         PhotoMatchStatus::Matched,
         PhotoMatchStatus::Confirmed},
        {QStringLiteral("unmatched-lower.jpg"),
         QStringLiteral("unmatched-lower.jpg"),
         false,
         {},
         PhotoMatchStatus::Confirmed,
         PhotoMatchStatus::Unmatched},
        {QStringLiteral("unconfirmed-lower.jpg"),
         QStringLiteral("unconfirmed-lower.jpg"),
         false,
         {},
         PhotoMatchStatus::Confirmed,
         PhotoMatchStatus::Matched},
        {QStringLiteral("confirmed-2.jpg"), QStringLiteral("confirmed-2.jpg"), false, {}, PhotoMatchStatus::Confirmed, PhotoMatchStatus::Confirmed},
    };
    navigationPhotoModel.setItems(std::move(navigationPhotos));
    MatchController navigationController;
    navigationController.setPhotoModel(&navigationPhotoModel);
    navigationController.setCurrentPhotoIndex(1);
    navigationController.nextUnmatchedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 3, QStringLiteral("下一张未匹配必须跳过待确认项，并识别裤裙未匹配的实拍图")))
    {
        return 1;
    }
    navigationController.previousUnmatchedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 0, QStringLiteral("上一张未匹配必须识别上衣未匹配的实拍图")))
    {
        return 1;
    }
    navigationController.setCurrentPhotoIndex(3);
    navigationController.previousUnconfirmedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 2, QStringLiteral("上一张未确认必须定位存在仅匹配款号的实拍图")))
    {
        return 1;
    }
    navigationController.nextUnconfirmedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 3, QStringLiteral("下一张未确认必须把任一部位未匹配的实拍图视为符合条件")))
    {
        return 1;
    }
    navigationController.nextUnconfirmedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 4, QStringLiteral("下一张未确认必须把任一部位仅匹配的实拍图视为符合条件")))
    {
        return 1;
    }
    navigationController.nextUnmatchedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 4, QStringLiteral("没有后续未匹配实拍图时必须保持当前选择")))
    {
        return 1;
    }
    navigationController.setCurrentPhotoIndex(1);
    navigationController.activatePreview(false);
    navigationController.nextUnmatchedPhoto();
    if (!check(navigationController.currentPhotoIndex() == 1, QStringLiteral("输出 Tab 激活时未匹配导航不得改变实拍图选择")))
    {
        return 1;
    }

    PhotoListModel     photoModel;
    QVector<PhotoItem> photos;
    photos.push_back({QStringLiteral("photo-1.jpg"), root.filePath(QStringLiteral("photo-1.jpg")), false});
    photos.push_back({QStringLiteral("photo-2.jpg"), root.filePath(QStringLiteral("photo-2.jpg")), false});
    photoModel.setItems(std::move(photos));
    controller.setPhotoModel(&photoModel);
    controller.setCurrentPhotoIndex(0);

    if (!check(photoModel.data(photoModel.index(0), PhotoListModel::UpperMatchStatusRole).toInt() == static_cast<int>(PhotoMatchStatus::Unmatched) &&
                   photoModel.data(photoModel.index(0), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Unmatched),
               QStringLiteral("无款号记录的实拍图上衣和裤裙状态都必须是未匹配")))
    {
        return 1;
    }

    if (!check(controller.previousPhotoPath().isEmpty() && controller.nextPhotoPath() == root.filePath(QStringLiteral("photo-2.jpg")),
               QStringLiteral("第一张实拍图只能预览下一张相邻图片")))
    {
        return 1;
    }

    controller.setInputFilterText(QStringLiteral("PHOTO-2"));
    if (!check(photoModel.rowCount() == 1 && photoModel.at(0)->fileName == QStringLiteral("photo-2.jpg") && photoModel.allItems().size() == 2 &&
                   controller.currentPhotoIndex() == 0,
               QStringLiteral("输入列表过滤只能影响显示，批量自动匹配仍必须保留全部实拍图")))
    {
        return 1;
    }
    if (!check(controller.metaObject()->indexOfMethod("autoMatchAllStyleIds()") >= 0, QStringLiteral("控制器必须向 QML 暴露批量自动匹配入口")))
    {
        return 1;
    }
    if (!check(controller.metaObject()->indexOfMethod("cancelAutoMatchAllStyleIds()") >= 0,
               QStringLiteral("控制器必须向 QML 暴露停止批量自动匹配入口")))
    {
        return 1;
    }
    controller.setInputFilterText(QString());
    if (!check(photoModel.rowCount() == 2, QStringLiteral("清空输入过滤关键字后应恢复全部实拍图片")))
    {
        return 1;
    }

    controller.activatePreview(false);
    if (!check(controller.currentImagePath() == root.filePath(QStringLiteral("Alpha/01.jpg")) && controller.currentImageCount() == 2,
               QStringLiteral("切到输出 Tab 应恢复当前归类项及其图片列表")))
    {
        return 1;
    }
    controller.nextImage(false);
    if (!check(controller.currentImagePage() == 1, QStringLiteral("输出 Tab 的下一张应切换归类目录内图片")))
    {
        return 1;
    }

    controller.activatePreview(true);
    if (!check(controller.currentImagePath() == root.filePath(QStringLiteral("photo-1.jpg")) && controller.currentImageCount() == 1,
               QStringLiteral("切到输入 Tab 应恢复当前选中的实拍图")))
    {
        return 1;
    }
    controller.nextImage(true);
    if (!check(controller.currentPhotoIndex() == 1 && controller.currentImagePath() == root.filePath(QStringLiteral("photo-2.jpg")) &&
                   controller.previousPhotoPath() == root.filePath(QStringLiteral("photo-1.jpg")) && controller.nextPhotoPath().isEmpty(),
               QStringLiteral("输入 Tab 的下一张应切换实拍图列表项并更新相邻图片预览")))
    {
        return 1;
    }

    const QString photoDir = root.filePath(QStringLiteral("photos"));
    if (!check(root.mkpath(QStringLiteral("photos/z")) && createFile(root.filePath(QStringLiteral("photos/input.jpg"))) &&
                   createFile(root.filePath(QStringLiteral("photos/second.jpg"))) && createFile(root.filePath(QStringLiteral("photos/z/third.jpg"))),
               QStringLiteral("无法创建实拍图片目录")))
    {
        return 1;
    }
    controller.setPhotoDir(photoDir);
    const QString nestedPhotoDisplay = photoModel.data(photoModel.index(2), PhotoListModel::DisplayLineRole).toString();
    if (!check(nestedPhotoDisplay == QDir::toNativeSeparators(QStringLiteral("z/third.jpg")),
               QStringLiteral("实拍图片列表应显示相对于实拍图片目录的路径，实际为: %1").arg(nestedPhotoDisplay)))
    {
        return 1;
    }

    const QString     databasePath = root.filePath(QStringLiteral("photos/gsm.db"));
    StoredMatchResult previousResult;
    previousResult.upper = {QStringLiteral("PREVIOUS-UPPER"), QStringLiteral("previous-upper.png"), true};
    previousResult.lower = {QStringLiteral("PREVIOUS-LOWER"), QStringLiteral("previous-lower.png"), true};
    StoredMatchResult currentResult;
    currentResult.lower = {QStringLiteral("CURRENT-LOWER"), QStringLiteral("current-lower.png"), true};
    StoredMatchResult nextResult;
    nextResult.upper = {QStringLiteral("NEXT-UPPER"), QStringLiteral("next-upper.png"), true};
    nextResult.lower = {QStringLiteral("NEXT-LOWER"), QStringLiteral("next-lower.png"), true};
    QString matchStoreError;
    if (!check(MatchResultStore::save(databasePath, photoModel.at(0)->imagePath, previousResult, &matchStoreError) &&
                   MatchResultStore::save(databasePath, photoModel.at(1)->imagePath, currentResult, &matchStoreError) &&
                   MatchResultStore::save(databasePath, photoModel.at(2)->imagePath, nextResult, &matchStoreError),
               QStringLiteral("无法准备相邻款号复制测试数据: %1").arg(matchStoreError)))
    {
        return 1;
    }

    QString autoMatchPolicyMessage;
    QObject::connect(&controller, &MatchController::logMessage, &controller, [&autoMatchPolicyMessage](const QString &message) {
        autoMatchPolicyMessage = message;
    });
    controller.setCurrentPhotoIndex(1);
    controller.setCurrentPhotoIndex(0);
    controller.setCurrentPhotoIndex(2);
    controller.setCurrentPhotoIndex(1);
    if (!check(controller.previousPhotoUpperMatchStatus() == static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   controller.previousPhotoLowerMatchStatus() == static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   controller.nextPhotoUpperMatchStatus() == static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   controller.nextPhotoLowerMatchStatus() == static_cast<int>(PhotoMatchStatus::Confirmed),
               QStringLiteral("相邻实拍预览必须暴露上一张和下一张的上衣、裤裙匹配状态")))
    {
        return 1;
    }
    controller.setCurrentPhotoIndex(0);
    if (!check(photoModel.data(photoModel.index(0), PhotoListModel::UpperMatchStatusRole).toInt() == static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   photoModel.data(photoModel.index(0), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Confirmed),
               QStringLiteral("选中已确认图片后，列表必须恢复上衣和裤裙的已确认状态")))
    {
        return 1;
    }
    controller.autoMatchStyleIds();
    if (!check(!controller.busy() && autoMatchPolicyMessage.contains(QStringLiteral("均已确认，已跳过自动匹配")),
               QStringLiteral("当前实拍图的上衣和裤裙均确认时不得启动推理")))
    {
        return 1;
    }

    QEventLoop batchMatchLoop;
    QObject::connect(&controller, &MatchController::busyChanged, &batchMatchLoop, [&] {
        if (!controller.busy())
        {
            batchMatchLoop.quit();
        }
    });
    controller.autoMatchAllStyleIds();
    if (!check(controller.busy(), QStringLiteral("批量匹配应在后台检查每张实拍图的确认状态")))
    {
        return 1;
    }
    QTimer::singleShot(kAsyncTimeoutMs, &batchMatchLoop, &QEventLoop::quit);
    batchMatchLoop.exec();
    if (!check(!controller.busy() && autoMatchPolicyMessage.contains(QStringLiteral("成功 0 张，跳过 2 张，失败 1 张")),
               QStringLiteral("批量匹配必须跳过两项均确认的实拍图，仅处理仍有未确认部位的图片")))
    {
        return 1;
    }

    controller.autoMatchAllStyleIds();
    if (!check(controller.busy() && controller.batchAutoMatchInProgress(), QStringLiteral("批量匹配期间必须暴露可停止状态")))
    {
        return 1;
    }
    controller.setParallelMatchThreadCount(4);
    if (!check(controller.parallelMatchThreadCount() == 3 && QSettings().value(QStringLiteral("matching/parallelThreads")).toInt() == 3,
               QStringLiteral("批量匹配期间不得切换或持久化并行匹配线程数")))
    {
        return 1;
    }
    controller.cancelAutoMatchAllStyleIds();
    QEventLoop cancelledBatchLoop;
    QObject::connect(&controller, &MatchController::batchAutoMatchInProgressChanged, &cancelledBatchLoop, [&] {
        if (!controller.batchAutoMatchInProgress())
        {
            cancelledBatchLoop.quit();
        }
    });
    QTimer::singleShot(kAsyncTimeoutMs, &cancelledBatchLoop, &QEventLoop::quit);
    cancelledBatchLoop.exec();
    if (!check(!controller.busy() && !controller.batchAutoMatchInProgress() && autoMatchPolicyMessage.contains(QStringLiteral("批量自动匹配已停止")),
               QStringLiteral("停止请求必须结束批量匹配并恢复按钮状态")))
    {
        return 1;
    }
    controller.setParallelMatchThreadCount(4);
    if (!check(controller.parallelMatchThreadCount() == 4, QStringLiteral("空闲状态下应允许切换并行匹配线程数")))
    {
        return 1;
    }

    controller.setCurrentPhotoIndex(0);
    controller.setCurrentPhotoIndex(1);
    if (!check(controller.copyAdjacentStyleIds(-1, QStringLiteral("upper"), QStringLiteral("cancel")), QStringLiteral("应能复制上一张的上衣款号")))
    {
        return 1;
    }
    auto copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER") && !copiedResult->upper.confirmed &&
                   copiedResult->lower.styleId == QStringLiteral("CURRENT-LOWER") && copiedResult->lower.confirmed,
               QStringLiteral("只复制上衣时应将上衣设为待确认，并保留当前裤裙记录")))
    {
        return 1;
    }
    if (!check(photoModel.data(photoModel.index(1), PhotoListModel::UpperMatchStatusRole).toInt() == static_cast<int>(PhotoMatchStatus::Matched) &&
                   photoModel.data(photoModel.index(1), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Confirmed),
               QStringLiteral("复制上衣款号后，列表必须立即显示上衣待确认、裤裙已确认")))
    {
        return 1;
    }

    if (!check(controller.copyWouldOverwriteConfirmedStyleIds(1, QStringLiteral("all"), false),
               QStringLiteral("覆盖当前图已确认的裤裙款号前应要求用户确认")))
    {
        return 1;
    }
    if (!check(!controller.copyAdjacentStyleIds(1, QStringLiteral("all"), QStringLiteral("cancel")),
               QStringLiteral("选择什么都不干时不得覆盖当前图已确认的款号")))
    {
        return 1;
    }
    auto protectedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(protectedResult && protectedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER") &&
                   protectedResult->lower.styleId == QStringLiteral("CURRENT-LOWER") && protectedResult->lower.confirmed,
               QStringLiteral("取消覆盖后当前图款号必须保持不变")))
    {
        return 1;
    }
    if (!check(controller.copyAdjacentStyleIds(1, QStringLiteral("all"), QStringLiteral("unconfirmedOnly")),
               QStringLiteral("应能只覆盖当前图未确认的款号")))
    {
        return 1;
    }
    copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("NEXT-UPPER") && !copiedResult->upper.confirmed &&
                   copiedResult->lower.styleId == QStringLiteral("CURRENT-LOWER") && copiedResult->lower.confirmed,
               QStringLiteral("仅覆盖未确认项时必须替换待确认上衣并保留已确认裤裙")))
    {
        return 1;
    }
    if (!check(controller.copyAdjacentStyleIds(1, QStringLiteral("all"), QStringLiteral("overwriteAll")), QStringLiteral("应能复制下一张的全部款号")))
    {
        return 1;
    }
    copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("NEXT-UPPER") &&
                   copiedResult->lower.styleId == QStringLiteral("NEXT-LOWER") && !copiedResult->upper.confirmed && !copiedResult->lower.confirmed,
               QStringLiteral("复制全部款号时应替换上衣和裤裙，并将两者设为待确认")))
    {
        return 1;
    }

    if (!check(controller.copyWouldOverwriteConfirmedStyleIds(-1, QStringLiteral("upper"), true),
               QStringLiteral("覆盖上一张已确认的上衣款号前应要求用户确认")))
    {
        return 1;
    }
    if (!check(!controller.copyStyleIdsToAdjacent(-1, QStringLiteral("upper"), QStringLiteral("cancel")),
               QStringLiteral("选择什么都不干时不得覆盖上一张已确认的上衣款号")))
    {
        return 1;
    }
    protectedResult = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(protectedResult && protectedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER") && protectedResult->upper.confirmed,
               QStringLiteral("取消覆盖后上一张款号必须保持不变")))
    {
        return 1;
    }
    if (!check(!controller.copyStyleIdsToAdjacent(-1, QStringLiteral("upper"), QStringLiteral("unconfirmedOnly")),
               QStringLiteral("目标部位已确认时，仅覆盖未确认项不应修改记录")))
    {
        return 1;
    }
    if (!check(controller.copyStyleIdsToAdjacent(-1, QStringLiteral("upper"), QStringLiteral("overwriteAll")),
               QStringLiteral("应能把当前上衣款号复制到上一张")))
    {
        return 1;
    }
    auto targetResult = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(targetResult && targetResult->upper.styleId == QStringLiteral("NEXT-UPPER") && !targetResult->upper.confirmed &&
                   targetResult->lower.styleId == QStringLiteral("PREVIOUS-LOWER") && targetResult->lower.confirmed,
               QStringLiteral("只复制上衣到上一张时应保留目标裤裙记录")))
    {
        return 1;
    }
    if (!check(controller.previousPhotoUpperMatchStatus() == static_cast<int>(PhotoMatchStatus::Matched) &&
                   controller.previousPhotoLowerMatchStatus() == static_cast<int>(PhotoMatchStatus::Confirmed),
               QStringLiteral("相邻实拍图的款号状态变更后，预览标记必须立即更新")))
    {
        return 1;
    }

    if (!check(controller.copyWouldOverwriteConfirmedStyleIds(1, QStringLiteral("all"), true),
               QStringLiteral("覆盖下一张已确认款号前应要求用户确认")))
    {
        return 1;
    }
    if (!check(!controller.copyStyleIdsToAdjacent(1, QStringLiteral("all"), QStringLiteral("cancel")),
               QStringLiteral("选择什么都不干时不得覆盖下一张已确认款号")))
    {
        return 1;
    }
    auto partiallyConfirmedTarget = MatchResultStore::load(databasePath, photoModel.at(2)->imagePath, &matchStoreError);
    if (!check(partiallyConfirmedTarget.has_value(), QStringLiteral("应能读取下一张款号以准备仅覆盖未确认项测试")))
    {
        return 1;
    }
    partiallyConfirmedTarget->upper = {QStringLiteral("STALE-UPPER"), QStringLiteral("stale-upper.png"), false};
    if (!check(MatchResultStore::save(databasePath, photoModel.at(2)->imagePath, *partiallyConfirmedTarget, &matchStoreError),
               QStringLiteral("无法准备下一张的待确认上衣款号: %1").arg(matchStoreError)))
    {
        return 1;
    }
    if (!check(controller.copyStyleIdsToAdjacent(1, QStringLiteral("all"), QStringLiteral("unconfirmedOnly")),
               QStringLiteral("应能只覆盖下一张未确认的款号")))
    {
        return 1;
    }
    targetResult = MatchResultStore::load(databasePath, photoModel.at(2)->imagePath, &matchStoreError);
    if (!check(targetResult && targetResult->upper.styleId == QStringLiteral("NEXT-UPPER") && !targetResult->upper.confirmed &&
                   targetResult->lower.styleId == QStringLiteral("NEXT-LOWER") && targetResult->lower.confirmed,
               QStringLiteral("仅覆盖未确认项时必须替换目标待确认上衣并保留已确认裤裙")))
    {
        return 1;
    }
    if (!check(controller.copyStyleIdsToAdjacent(1, QStringLiteral("all"), QStringLiteral("overwriteAll")),
               QStringLiteral("应能把当前全部款号复制到下一张")))
    {
        return 1;
    }
    targetResult = MatchResultStore::load(databasePath, photoModel.at(2)->imagePath, &matchStoreError);
    if (!check(targetResult && targetResult->upper.styleId == QStringLiteral("NEXT-UPPER") &&
                   targetResult->lower.styleId == QStringLiteral("NEXT-LOWER") && !targetResult->upper.confirmed && !targetResult->lower.confirmed,
               QStringLiteral("复制全部款号到下一张时应替换目标两部分并设为待确认")))
    {
        return 1;
    }

    const auto beforeBoundaryCopy = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    controller.setCurrentPhotoIndex(0);
    if (!check(!controller.copyAdjacentStyleIds(-1, QStringLiteral("all"), QStringLiteral("cancel")),
               QStringLiteral("第一张实拍图不应允许复制上一张款号")))
    {
        return 1;
    }
    if (!check(!controller.copyStyleIdsToAdjacent(-1, QStringLiteral("all"), QStringLiteral("cancel")),
               QStringLiteral("第一张实拍图不应允许把款号复制到上一张")))
    {
        return 1;
    }
    const auto unchangedPrevious = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(beforeBoundaryCopy && unchangedPrevious && unchangedPrevious->upper.styleId == beforeBoundaryCopy->upper.styleId &&
                   unchangedPrevious->upper.confirmed == beforeBoundaryCopy->upper.confirmed &&
                   unchangedPrevious->lower.styleId == beforeBoundaryCopy->lower.styleId &&
                   unchangedPrevious->lower.confirmed == beforeBoundaryCopy->lower.confirmed,
               QStringLiteral("越界复制失败时不得修改当前图片的款号记录")))
    {
        return 1;
    }

    controller.setCurrentPhotoIndex(1);
    controller.confirmAutoMatch(QStringLiteral("upper"));
    controller.rejectAutoMatch(QStringLiteral("lower"));
    if (!check(photoModel.data(photoModel.index(1), PhotoListModel::UpperMatchStatusRole).toInt() == static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   photoModel.data(photoModel.index(1), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Unmatched),
               QStringLiteral("确认上衣并删除裤裙匹配后，列表必须立即显示已确认和未匹配状态")))
    {
        return 1;
    }
    controller.setCurrentIndex(0);
    controller.setCurrentImagePage(1);
    controller.activatePreview(true);
    if (!check(QSettings().value(QStringLiteral("photo/lastDir")).toString() == photoDir, QStringLiteral("实拍图片目录应持久化保存")))
    {
        return 1;
    }

    CandidateListModel restoredCandidateModel;
    PhotoListModel     restoredPhotoModel;
    MatchController    restoredController;
    restoredController.setCandidateModel(&restoredCandidateModel);
    restoredController.setPhotoModel(&restoredPhotoModel);
    restoredController.restorePersistentState();
    if (!check(restoredController.photoDir() == photoDir && restoredPhotoModel.rowCount() == 3, QStringLiteral("启动恢复后应自动加载实拍图片目录")))
    {
        return 1;
    }
    if (!check(restoredPhotoModel.data(restoredPhotoModel.index(0), PhotoListModel::UpperMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Matched) &&
                   restoredPhotoModel.data(restoredPhotoModel.index(0), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   restoredPhotoModel.data(restoredPhotoModel.index(1), PhotoListModel::UpperMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Confirmed) &&
                   restoredPhotoModel.data(restoredPhotoModel.index(1), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Unmatched) &&
                   restoredPhotoModel.data(restoredPhotoModel.index(2), PhotoListModel::UpperMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Matched) &&
                   restoredPhotoModel.data(restoredPhotoModel.index(2), PhotoListModel::LowerMatchStatusRole).toInt() ==
                       static_cast<int>(PhotoMatchStatus::Matched),
               QStringLiteral("启动扫描必须从 gsm.db 恢复每张实拍图两个部位的匹配状态")))
    {
        return 1;
    }
    if (!check(restoredController.outputDir() == temporary.path() && restoredCandidateModel.rowCount() == 3,
               QStringLiteral("启动恢复后应自动加载输出目录")))
    {
        return 1;
    }
    if (!check(restoredController.currentPhotoIndex() == 1 && restoredController.currentIndex() == 0,
               QStringLiteral("启动恢复后应还原输入和输出列表的选中项")))
    {
        return 1;
    }
    if (!check(restoredController.inputTabActive() && restoredController.currentImagePath() == root.filePath(QStringLiteral("photos/second.jpg")),
               QStringLiteral("启动恢复后应还原激活的输入 Tab 及其主图")))
    {
        return 1;
    }
    restoredController.activatePreview(false);
    if (!check(restoredController.currentImagePage() == 1 && restoredController.currentImagePath() == root.filePath(QStringLiteral("Alpha/02.PNG")),
               QStringLiteral("启动恢复后应还原输出 Tab 当前浏览的主图")))
    {
        return 1;
    }

    return 0;
}
