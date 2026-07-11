#include "CandidateListModel.h"
#include "MatchController.h"
#include "PhotoListModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

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
    QTemporaryDir temporary;
    QTemporaryDir settingsTemporary;
    if (!check(temporary.isValid() && settingsTemporary.isValid(),
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
                   && controller.currentPhotoIndex() == 0,
               QStringLiteral("输入列表应即时按文件名进行不区分大小写的关键字过滤")))
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

    controller.setCurrentPhotoIndex(0);
    controller.setCurrentPhotoIndex(1);
    if (!check(controller.copyAdjacentStyleIds(-1, QStringLiteral("upper")),
               QStringLiteral("应能复制上一张的上衣款号")))
        return 1;
    auto copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("PREVIOUS-UPPER")
                   && !copiedResult->upper.confirmed && copiedResult->lower.styleId == QStringLiteral("CURRENT-LOWER")
                   && copiedResult->lower.confirmed,
               QStringLiteral("只复制上衣时应将上衣设为待确认，并保留当前裤裙记录")))
        return 1;

    if (!check(controller.copyAdjacentStyleIds(1, QStringLiteral("all")),
               QStringLiteral("应能复制下一张的全部款号")))
        return 1;
    copiedResult = MatchResultStore::load(databasePath, photoModel.at(1)->imagePath, &matchStoreError);
    if (!check(copiedResult && copiedResult->upper.styleId == QStringLiteral("NEXT-UPPER")
                   && copiedResult->lower.styleId == QStringLiteral("NEXT-LOWER")
                   && !copiedResult->upper.confirmed && !copiedResult->lower.confirmed,
               QStringLiteral("复制全部款号时应替换上衣和裤裙，并将两者设为待确认")))
        return 1;

    controller.setCurrentPhotoIndex(0);
    if (!check(!controller.copyAdjacentStyleIds(-1, QStringLiteral("all")),
               QStringLiteral("第一张实拍图不应允许复制上一张款号")))
        return 1;
    const auto unchangedPrevious = MatchResultStore::load(databasePath, photoModel.at(0)->imagePath, &matchStoreError);
    if (!check(unchangedPrevious && unchangedPrevious->upper.confirmed && unchangedPrevious->lower.confirmed,
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
