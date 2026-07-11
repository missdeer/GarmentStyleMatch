#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "GalleryListModel.h"

namespace
{

    bool check(bool condition, const QString &message)
    {
        if (!condition)
            qCritical().noquote() << message;
        return condition;
    }

    bool isSelected(const GalleryListModel &model, int row)
    {
        return model.data(model.index(row), GalleryListModel::SelectedRole).toBool();
    }

    bool writeFile(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("image") == 5;
    }

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    GalleryListModel model;
    model.setItems({
        {QStringLiteral("STYLE100A"), QStringLiteral("a.png"), QStringLiteral("baby")},
        {QStringLiteral("STYLE200B"), QStringLiteral("b.png"), QStringLiteral("baby")},
        {QStringLiteral("STYLE300C"), QStringLiteral("c.png"), QStringLiteral("baby")},
    });

    model.toggleSelected(0);
    if (!check(model.selectedIndex() == 0 && isSelected(model, 0) && !isSelected(model, 1), QStringLiteral("点击未选项后应只选中该项")))
        return 1;

    model.toggleSelected(1);
    if (!check(model.selectedIndex() == 1 && !isSelected(model, 0) && isSelected(model, 1), QStringLiteral("切换选项后旧项必须自动取消")))
        return 1;

    model.toggleSelected(1);
    if (!check(model.selectedIndex() == -1 && !isSelected(model, 0) && !isSelected(model, 1), QStringLiteral("再次点击当前项后应允许不选")))
        return 1;

    model.toggleSelected(2);
    model.clearSelection();
    if (!check(model.selectedIndex() == -1 && !isSelected(model, 2), QStringLiteral("clearSelection 应清除当前选择")))
        return 1;

    model.setFilterText(QStringLiteral("style2"));
    if (!check(model.rowCount() == 1 && model.at(0)->styleId == QStringLiteral("STYLE200B"),
               QStringLiteral("搜索应即时按款号进行不区分大小写的包含匹配")))
        return 1;

    model.setFilterText(QStringLiteral("missing"));
    if (!check(model.rowCount() == 0, QStringLiteral("无匹配款号时图库应为空")))
        return 1;
    if (!check(model.allItems().size() == 3, QStringLiteral("自动匹配必须使用完整图库，不能被界面搜索过滤缩小范围")))
        return 1;

    model.setFilterText(QStringLiteral("  "));
    if (!check(model.rowCount() == 3, QStringLiteral("清空搜索内容后应恢复全部款号")))
        return 1;

    QTemporaryDir temporary;
    if (!check(temporary.isValid(), QStringLiteral("无法创建缓存加载测试目录")))
        return 1;
    const QString stylesDir = temporary.filePath(QStringLiteral("styles"));
    if (!check(writeFile(QDir(stylesDir).filePath(QStringLiteral("STYLE100A/001.png"))) &&
                   writeFile(QDir(stylesDir).filePath(QStringLiteral("STYLE100A/002.png"))) &&
                   writeFile(QDir(stylesDir).filePath(QStringLiteral("STYLE200B/001.jpg"))) &&
                   writeFile(QDir(stylesDir).filePath(QStringLiteral("LEGACY300C.png"))),
               QStringLiteral("无法创建款式缓存测试数据")))
        return 1;

    model.loadFromStyleCacheDir(stylesDir);
    if (!check(model.rowCount() == 3, QStringLiteral("只应加载款号子目录中的 3 张图片，styles 根目录图片必须忽略")))
        return 1;
    if (!check(model.at(0)->styleId == QStringLiteral("STYLE100A") && model.at(1)->styleId == QStringLiteral("STYLE100A") &&
                   model.at(2)->styleId == QStringLiteral("STYLE200B"),
               QStringLiteral("缓存图片必须使用父目录名作为款号")))
        return 1;

    return 0;
}
