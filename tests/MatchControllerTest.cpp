#include "CandidateListModel.h"
#include "MatchController.h"
#include "PhotoListModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
    if (!check(temporary.isValid(), QStringLiteral("无法创建临时目录")))
        return 1;

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

    return 0;
}
