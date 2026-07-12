#include "CandidateListModel.h"
#include "MatchController.h"
#include "PhotoListModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

namespace {

bool check(bool condition, const QString &message)
{
    if (condition)
        return true;
    std::cerr << message.toStdString() << '\n';
    return false;
}

bool createFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temporary;
    QTemporaryDir settingsTemporary;
    QTemporaryDir modelTemporary;
    if (!check(temporary.isValid() && settingsTemporary.isValid() && modelTemporary.isValid(),
               QStringLiteral("无法创建临时目录")))
        return 1;

    QCoreApplication::setOrganizationName(QStringLiteral("EidosTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MatchControllerTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsTemporary.path());
    QSettings().clear();

    QDir root(temporary.path());
    if (!check(root.mkpath(QStringLiteral("Alpha/nested"))
                   && root.mkpath(QStringLiteral("Beta")),
               QStringLiteral("无法创建测试子目录")))
        return 1;
    if (!check(createFile(root.filePath(QStringLiteral("Alpha/01.jpg")))
                   && createFile(root.filePath(QStringLiteral("Alpha/02.PNG")))
                   && createFile(root.filePath(QStringLiteral("Alpha/readme.txt")))
                   && createFile(root.filePath(QStringLiteral("Alpha/nested/03.jpg")))
                   && createFile(root.filePath(QStringLiteral("root.jpg"))),
               QStringLiteral("无法创建测试文件")))
        return 1;

    CandidateListModel model;
    MatchController controller;
    controller.setCandidateModel(&model);
    controller.setOutputDir(temporary.path());

    const QString expectedModelDirectory =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .absoluteFilePath(QStringLiteral("models"));
    if (!check(controller.modelDirectory() == expectedModelDirectory,
               QStringLiteral("下载模型必须保存到应用的 LocalAppData/models 目录")))
        return 1;

#ifdef Q_OS_MACOS
    const QString expectedApplicationModelDirectory =
        QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../Resources/models")));
#else
    const QString expectedApplicationModelDirectory =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("models"));
#endif
    if (!check(controller.applicationModelDirectory() == expectedApplicationModelDirectory,
               QStringLiteral("随应用打包的模型目录必须符合当前平台的应用布局")))
        return 1;

    QDir modelRoot(modelTemporary.path());
    const QString applicationModelsDir = modelRoot.filePath(QStringLiteral("application-models"));
    const QString localModelsDir = modelRoot.filePath(QStringLiteral("local-models"));
    if (!check(modelRoot.mkpath(QStringLiteral("application-models"))
                   && modelRoot.mkpath(QStringLiteral("local-models"))
                   && createFile(QDir(applicationModelsDir).filePath(QStringLiteral("clothes_segformer_b2.onnx")))
                   && createFile(QDir(applicationModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx")))
                   && createFile(QDir(localModelsDir).filePath(QStringLiteral("clothes_segformer_b2.onnx")))
                   && createFile(QDir(localModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx"))),
               QStringLiteral("无法创建模型查找顺序测试文件")))
        return 1;
    if (!check(MatchController::findAvailableModelDirectory(applicationModelsDir, localModelsDir) == applicationModelsDir,
               QStringLiteral("匹配必须优先使用 applicationDir/models 中的完整模型")))
        return 1;
    QFile::remove(QDir(applicationModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx")));
    if (!check(MatchController::findAvailableModelDirectory(applicationModelsDir, localModelsDir) == localModelsDir,
               QStringLiteral("应用目录模型不完整时必须回退到 LocalAppData/models")))
        return 1;
    QFile::remove(QDir(localModelsDir).filePath(QStringLiteral("fashion_clip_vision.onnx")));
    if (!check(MatchController::findAvailableModelDirectory(applicationModelsDir, localModelsDir).isEmpty(),
               QStringLiteral("两个目录的模型都不完整时必须报告无可用模型")))
        return 1;

#ifdef Q_OS_WIN
    bool modelDownloadStopped = false;
    QString lastModelDownloadMessage;
    QObject::connect(&controller, &MatchController::logMessage, &controller, [&modelDownloadStopped, &lastModelDownloadMessage](const QString &message) {
        lastModelDownloadMessage = message;
        if (message == QStringLiteral("模型下载已停止"))
            modelDownloadStopped = true;
    });
    QEventLoop downloadStopLoop;
    QObject::connect(&controller, &MatchController::modelDownloadInProgressChanged, &downloadStopLoop, [&] {
        if (!controller.modelDownloadInProgress())
            downloadStopLoop.quit();
    });
    controller.downloadModels();
    if (!check(controller.modelDownloadInProgress(),
               QStringLiteral("触发模型下载后必须进入下载状态，实际消息：%1").arg(lastModelDownloadMessage)))
        return 1;
    QTimer::singleShot(0, &controller, &MatchController::cancelModelDownload);
    QTimer::singleShot(5000, &downloadStopLoop, &QEventLoop::quit);
    downloadStopLoop.exec();
    if (!check(!controller.modelDownloadInProgress() && modelDownloadStopped,
               QStringLiteral("停止下载必须终止下载进程并报告已停止")))
        return 1;
#endif

    if (!check(controller.availableInferenceEngines().contains(QStringLiteral("CPU")),
               QStringLiteral("推理引擎列表必须始终包含 CPU")))
        return 1;
    const QString activeInferenceEngine = controller.currentInferenceEngine();
    if (!check(controller.setCurrentInferenceEngine(QStringLiteral("CPU"))
                   || controller.currentInferenceEngine() == QStringLiteral("CPU"),
               QStringLiteral("应允许选择 CPU 推理引擎")))
        return 1;
    if (!check(QSettings().value(QStringLiteral("matching/provider")).toString()
                   == QStringLiteral("cpu"),
               QStringLiteral("推理引擎选择应持久化到 matching/provider")))
        return 1;
    if (!check(controller.currentInferenceEngine() == activeInferenceEngine,
               QStringLiteral("推理引擎设置在重启前不得改变当前进程使用的引擎")))
        return 1;
    if (!check(!controller.setCurrentInferenceEngine(QStringLiteral("unsupported")),
               QStringLiteral("不应接受本机不支持的推理引擎")))
        return 1;

    if (!check(QSettings().value(QStringLiteral("output/lastDir")).toString()
                   == temporary.path(),
               QStringLiteral("输出目录应持久化保存")))
        return 1;

    if (!check(model.rowCount() == 2,
               QStringLiteral("输出列表应只包含两个直接子目录")))
        return 1;
    const QString alphaDisplay = model.data(
        model.index(0), CandidateListModel::DisplayLineRole).toString();
    if (!check(alphaDisplay == QStringLiteral("Alpha(2)"),
               QStringLiteral("Alpha 条目应显示直接包含的两张图片，实际为: %1")
                   .arg(alphaDisplay)))
        return 1;
    if (!check(model.data(model.index(1), CandidateListModel::DisplayLineRole).toString()
                   == QStringLiteral("Beta(0)"),
               QStringLiteral("空子目录也应显示为一个条目")))
        return 1;
    if (!check(model.data(model.index(0), CandidateListModel::ImagePathRole).toString()
                   == root.filePath(QStringLiteral("Alpha/01.jpg")),
               QStringLiteral("条目预览应使用子目录中的第一张图片")))
        return 1;
    if (!check(controller.currentOutputImagePaths().size() == 2,
               QStringLiteral("控制器应暴露选中目录中的全部图片")))
        return 1;
    controller.setOutputFilterText(QStringLiteral("beta"));
    if (!check(model.rowCount() == 1
                   && model.at(0)->styleId == QStringLiteral("Beta")
                   && controller.currentIndex() == 0,
               QStringLiteral("输出列表应即时按款号进行不区分大小写的关键字过滤")))
        return 1;
    controller.setOutputFilterText(QString());
    if (!check(model.rowCount() == 2,
               QStringLiteral("清空输出过滤关键字后应恢复全部归类项")))
        return 1;
    controller.nextImage();
    if (!check(controller.currentImagePage() == 1
                   && controller.currentImagePath()
                       == root.filePath(QStringLiteral("Alpha/02.PNG")),
               QStringLiteral("下一张应同步切换页索引和主图路径")))
        return 1;
    controller.previousImage();
    if (!check(controller.currentImagePage() == 0
                   && controller.currentImagePath()
                       == root.filePath(QStringLiteral("Alpha/01.jpg")),
               QStringLiteral("上一张应同步切回前一个缩略图")))
        return 1;

    PhotoListModel photoModel;
    QVector<PhotoItem> photos;
    photos.push_back({QStringLiteral("photo-1.jpg"),
                      root.filePath(QStringLiteral("photo-1.jpg")), false});
    photos.push_back({QStringLiteral("photo-2.jpg"),
                      root.filePath(QStringLiteral("photo-2.jpg")), false});
    photoModel.setItems(std::move(photos));
    controller.setPhotoModel(&photoModel);
    controller.setCurrentPhotoIndex(0);

    controller.setInputFilterText(QStringLiteral("PHOTO-2"));
    if (!check(photoModel.rowCount() == 1
                   && photoModel.at(0)->fileName == QStringLiteral("photo-2.jpg")
                   && photoModel.allItems().size() == 2
                   && controller.currentPhotoIndex() == 0,
               QStringLiteral("输入列表过滤只能影响显示，批量自动匹配仍必须保留全部实拍图")))
        return 1;
    if (!check(controller.metaObject()->indexOfMethod("autoMatchAllStyleIds()") >= 0,
               QStringLiteral("控制器必须向 QML 暴露批量自动匹配入口")))
        return 1;
    if (!check(controller.metaObject()->indexOfMethod("cancelAutoMatchAllStyleIds()") >= 0,
               QStringLiteral("控制器必须向 QML 暴露停止批量自动匹配入口")))
        return 1;
    controller.setInputFilterText(QString());
    if (!check(photoModel.rowCount() == 2,
               QStringLiteral("清空输入过滤关键字后应恢复全部实拍图片")))
        return 1;

    controller.activatePreview(false);
    if (!check(controller.currentImagePath()
                   == root.filePath(QStringLiteral("Alpha/01.jpg"))
                   && controller.currentImageCount() == 2,
               QStringLiteral("切到输出 Tab 应恢复当前归类项及其图片列表")))
        return 1;
    controller.nextImage(false);
    if (!check(controller.currentImagePage() == 1,
               QStringLiteral("输出 Tab 的下一张应切换归类目录内图片")))
        return 1;

    controller.activatePreview(true);
    if (!check(controller.currentImagePath()
                   == root.filePath(QStringLiteral("photo-1.jpg"))
                   && controller.currentImageCount() == 1,
               QStringLiteral("切到输入 Tab 应恢复当前选中的实拍图")))
        return 1;
    controller.nextImage(true);
    if (!check(controller.currentPhotoIndex() == 1
                   && controller.currentImagePath()
                       == root.filePath(QStringLiteral("photo-2.jpg")),
               QStringLiteral("输入 Tab 的下一张应切换实拍图列表项")))
        return 1;

    const QString photoDir = root.filePath(QStringLiteral("photos"));
    if (!check(root.mkpath(QStringLiteral("photos/z"))
                   && createFile(root.filePath(QStringLiteral("photos/input.jpg")))
                   && createFile(root.filePath(QStringLiteral("photos/second.jpg")))
                   && createFile(root.filePath(QStringLiteral("photos/z/third.jpg"))),
               QStringLiteral("无法创建实拍图片目录")))
        return 1;
    controller.setPhotoDir(photoDir);
    const QString nestedPhotoDisplay = photoModel.data(
        photoModel.index(2), PhotoListModel::DisplayLineRole).toString();
    if (!check(nestedPhotoDisplay
                   == QDir::toNativeSeparators(QStringLiteral("z/third.jpg")),
               QStringLiteral("实拍图片列表应显示相对于实拍图片目录的路径，实际为: %1")
                   .arg(nestedPhotoDisplay)))
        return 1;

    const QString databasePath = root.filePath(QStringLiteral("photos/gsm.db"));
    StoredMatchResult previousResult;
    previousResult.upper = {QStringLiteral("PREVIOUS-UPPER"), QStringLiteral("previous-upper.png"), true};
    previousResult.lower = {QStringLiteral("PREVIOUS-LOWER"), QStringLiteral("previous-lower.png"), true};
    StoredMatchResult currentResult;
    currentResult.lower = {QStringLiteral("CURRENT-LOWER"), QStringLiteral("current-lower.png"), true};
    StoredMatchResult nextResult;
    nextResult.upper = {QStringLiteral("NEXT-UPPER"), QStringLiteral("next-upper.png"), true};
    nextResult.lower = {QStringLiteral("NEXT-LOWER"), QStringLiteral("next-lower.png"), true};
    QString matchStoreError;
    if (!check(MatchResultStore::save(databasePath, photoModel.at(0)->imagePath, previousResult, &matchStoreError)
                   && MatchResultStore::save(databasePath, photoModel.at(1)->imagePath, currentResult, &matchStoreError)
                   && MatchResultStore::save(databasePath, photoModel.at(2)->imagePath, nextResult, &matchStoreError),
               QStringLiteral("无法准备相邻款号复制测试数据: %1").arg(matchStoreError)))
        return 1;

    QString autoMatchPolicyMessage;
    QObject::connect(&controller, &MatchController::logMessage, &controller,
                     [&autoMatchPolicyMessage](const QString &message) { autoMatchPolicyMessage = message; });
    controller.setCurrentPhotoIndex(0);
    controller.autoMatchStyleIds();
    if (!check(!controller.busy() && autoMatchPolicyMessage.contains(QStringLiteral("均已确认，已跳过自动匹配")),
               QStringLiteral("当前实拍图的上衣和裤裙均确认时不得启动推理")))
        return 1;

    QEventLoop batchMatchLoop;
    QObject::connect(&controller, &MatchController::busyChanged, &batchMatchLoop, [&] {
        if (!controller.busy())
            batchMatchLoop.quit();
    });
    controller.autoMatchAllStyleIds();
    if (!check(controller.busy(), QStringLiteral("批量匹配应在后台检查每张实拍图的确认状态")))
        return 1;
    QTimer::singleShot(5000, &batchMatchLoop, &QEventLoop::quit);
    batchMatchLoop.exec();
    if (!check(!controller.busy()
                   && autoMatchPolicyMessage.contains(QStringLiteral("成功 0 张，跳过 2 张，失败 1 张")),
               QStringLiteral("批量匹配必须跳过两项均确认的实拍图，仅处理仍有未确认部位的图片")))
        return 1;

    controller.autoMatchAllStyleIds();
    if (!check(controller.busy() && controller.batchAutoMatchInProgress(),
               QStringLiteral("批量匹配期间必须暴露可停止状态")))
        return 1;
    controller.cancelAutoMatchAllStyleIds();
    QEventLoop cancelledBatchLoop;
    QObject::connect(&controller, &MatchController::batchAutoMatchInProgressChanged, &cancelledBatchLoop, [&] {
        if (!controller.batchAutoMatchInProgress())
            cancelledBatchLoop.quit();
    });
    QTimer::singleShot(5000, &cancelledBatchLoop, &QEventLoop::quit);
    cancelledBatchLoop.exec();
    if (!check(!controller.busy() && !controller.batchAutoMatchInProgress()
                   && autoMatchPolicyMessage.contains(QStringLiteral("批量自动匹配已停止")),
               QStringLiteral("停止请求必须结束批量匹配并恢复按钮状态")))
        return 1;

    controller.setCurrentPhotoIndex(0);
    controller.setCurrentPhotoIndex(1);
    if (!check(controller.copyAdjacentStyleIds(-1, QStringLiteral("upper"), false),
               QStringLiteral("应能复制上一张的上衣款号")))
        return 1;
    auto copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER")
                   && !copiedResult->upper.confirmed && copiedResult->lower.styleId == QStringLiteral("CURRENT-LOWER")
                   && copiedResult->lower.confirmed,
               QStringLiteral("只复制上衣时应将上衣设为待确认，并保留当前裤裙记录")))
        return 1;

    if (!check(controller.copyWouldOverwriteConfirmedStyleIds(1, QStringLiteral("all"), false),
               QStringLiteral("覆盖当前图已确认的裤裙款号前应要求用户确认")))
        return 1;
    if (!check(!controller.copyAdjacentStyleIds(1, QStringLiteral("all"), false),
               QStringLiteral("未经确认不得覆盖当前图已确认的款号")))
        return 1;
    auto protectedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(protectedResult && protectedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER")
                   && protectedResult->lower.styleId == QStringLiteral("CURRENT-LOWER") && protectedResult->lower.confirmed,
               QStringLiteral("取消覆盖后当前图款号必须保持不变")))
        return 1;
    if (!check(controller.copyAdjacentStyleIds(1, QStringLiteral("all"), true),
               QStringLiteral("应能复制下一张的全部款号")))
        return 1;
    copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("NEXT-UPPER")
                   && copiedResult->lower.styleId == QStringLiteral("NEXT-LOWER")
                   && !copiedResult->upper.confirmed && !copiedResult->lower.confirmed,
               QStringLiteral("复制全部款号时应替换上衣和裤裙，并将两者设为待确认")))
        return 1;

    if (!check(controller.copyWouldOverwriteConfirmedStyleIds(-1, QStringLiteral("upper"), true),
               QStringLiteral("覆盖上一张已确认的上衣款号前应要求用户确认")))
        return 1;
    if (!check(!controller.copyStyleIdsToAdjacent(-1, QStringLiteral("upper"), false),
               QStringLiteral("未经确认不得覆盖上一张已确认的上衣款号")))
        return 1;
    protectedResult = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(protectedResult && protectedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER")
                   && protectedResult->upper.confirmed,
               QStringLiteral("取消覆盖后上一张款号必须保持不变")))
        return 1;
    if (!check(controller.copyStyleIdsToAdjacent(-1, QStringLiteral("upper"), true),
               QStringLiteral("应能把当前上衣款号复制到上一张")))
        return 1;
    auto targetResult = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(targetResult && targetResult->upper.styleId == QStringLiteral("NEXT-UPPER")
                   && !targetResult->upper.confirmed && targetResult->lower.styleId == QStringLiteral("PREVIOUS-LOWER")
                   && targetResult->lower.confirmed,
               QStringLiteral("只复制上衣到上一张时应保留目标裤裙记录")))
        return 1;

    if (!check(controller.copyWouldOverwriteConfirmedStyleIds(1, QStringLiteral("all"), true),
               QStringLiteral("覆盖下一张已确认款号前应要求用户确认")))
        return 1;
    if (!check(!controller.copyStyleIdsToAdjacent(1, QStringLiteral("all"), false),
               QStringLiteral("未经确认不得覆盖下一张已确认款号")))
        return 1;
    if (!check(controller.copyStyleIdsToAdjacent(1, QStringLiteral("all"), true),
               QStringLiteral("应能把当前全部款号复制到下一张")))
        return 1;
    targetResult = MatchResultStore::load(databasePath, photoModel.at(2)->imagePath, &matchStoreError);
    if (!check(targetResult && targetResult->upper.styleId == QStringLiteral("NEXT-UPPER")
                   && targetResult->lower.styleId == QStringLiteral("NEXT-LOWER")
                   && !targetResult->upper.confirmed && !targetResult->lower.confirmed,
               QStringLiteral("复制全部款号到下一张时应替换目标两部分并设为待确认")))
        return 1;

    const auto beforeBoundaryCopy = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    controller.setCurrentPhotoIndex(0);
    if (!check(!controller.copyAdjacentStyleIds(-1, QStringLiteral("all"), false),
               QStringLiteral("第一张实拍图不应允许复制上一张款号")))
        return 1;
    if (!check(!controller.copyStyleIdsToAdjacent(-1, QStringLiteral("all"), false),
               QStringLiteral("第一张实拍图不应允许把款号复制到上一张")))
        return 1;
    const auto unchangedPrevious = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(beforeBoundaryCopy && unchangedPrevious
                   && unchangedPrevious->upper.styleId == beforeBoundaryCopy->upper.styleId
                   && unchangedPrevious->upper.confirmed == beforeBoundaryCopy->upper.confirmed
                   && unchangedPrevious->lower.styleId == beforeBoundaryCopy->lower.styleId
                   && unchangedPrevious->lower.confirmed == beforeBoundaryCopy->lower.confirmed,
               QStringLiteral("越界复制失败时不得修改当前图片的款号记录")))
        return 1;

    controller.setCurrentPhotoIndex(1);
    controller.setCurrentIndex(0);
    controller.setCurrentImagePage(1);
    controller.activatePreview(true);
    if (!check(QSettings().value(QStringLiteral("photo/lastDir")).toString()
                   == photoDir,
               QStringLiteral("实拍图片目录应持久化保存")))
        return 1;

    CandidateListModel restoredCandidateModel;
    PhotoListModel restoredPhotoModel;
    MatchController restoredController;
    restoredController.setCandidateModel(&restoredCandidateModel);
    restoredController.setPhotoModel(&restoredPhotoModel);
    restoredController.restorePersistentState();
    if (!check(restoredController.photoDir() == photoDir
                   && restoredPhotoModel.rowCount() == 3,
               QStringLiteral("启动恢复后应自动加载实拍图片目录")))
        return 1;
    if (!check(restoredController.outputDir() == temporary.path()
                   && restoredCandidateModel.rowCount() == 3,
               QStringLiteral("启动恢复后应自动加载输出目录")))
        return 1;
    if (!check(restoredController.currentPhotoIndex() == 1
                   && restoredController.currentIndex() == 0,
               QStringLiteral("启动恢复后应还原输入和输出列表的选中项")))
        return 1;
    if (!check(restoredController.inputTabActive()
                   && restoredController.currentImagePath()
                       == root.filePath(QStringLiteral("photos/second.jpg")),
               QStringLiteral("启动恢复后应还原激活的输入 Tab 及其主图")))
        return 1;
    restoredController.activatePreview(false);
    if (!check(restoredController.currentImagePage() == 1
                   && restoredController.currentImagePath()
                       == root.filePath(QStringLiteral("Alpha/02.PNG")),
               QStringLiteral("启动恢复后应还原输出 Tab 当前浏览的主图")))
        return 1;

    return 0;
}
