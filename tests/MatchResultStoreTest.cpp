#include <iostream>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "MatchResultStore.h"

namespace
{
    bool check(bool condition, const QString &message)
    {
        if (!condition)
            std::cerr << message.toStdString() << '\n';
        return condition;
    }
} // namespace

int main()
{
    StoredMatchResult confirmedUpper;
    confirmedUpper.upper = {QStringLiteral("CONFIRMED-UPPER"), QStringLiteral("confirmed-upper.png"), true};
    confirmedUpper.lower = {QStringLiteral("OLD-LOWER"), QStringLiteral("old-lower.png"), false};
    StoredMatchResult replacement;
    replacement.upper = {QStringLiteral("NEW-UPPER"), QStringLiteral("new-upper.png"), false};
    replacement.lower = {QStringLiteral("NEW-LOWER"), QStringLiteral("new-lower.png"), false};
    confirmedUpper.replaceUnconfirmedMatches(replacement);
    if (!check(confirmedUpper.upper.styleId == QStringLiteral("CONFIRMED-UPPER") && confirmedUpper.upper.confirmed
                   && confirmedUpper.lower.styleId == QStringLiteral("NEW-LOWER") && !confirmedUpper.lower.confirmed,
               QStringLiteral("自动匹配只能覆盖未确认部位，已确认上衣必须保持不变")))
        return 1;

    StoredMatchResult confirmedBoth;
    confirmedBoth.upper = {QStringLiteral("CONFIRMED-UPPER"), QStringLiteral("confirmed-upper.png"), true};
    confirmedBoth.lower = {QStringLiteral("CONFIRMED-LOWER"), QStringLiteral("confirmed-lower.png"), true};
    if (!check(confirmedBoth.allMatchesConfirmed(), QStringLiteral("上衣和裤裙均确认时必须跳过自动匹配")))
        return 1;

    StoredMatchResult confirmedNeither;
    confirmedNeither.replaceUnconfirmedMatches(replacement);
    if (!check(confirmedNeither.upper.styleId == QStringLiteral("NEW-UPPER")
                   && confirmedNeither.lower.styleId == QStringLiteral("NEW-LOWER"),
               QStringLiteral("上衣和裤裙均未确认时必须同时覆盖两项匹配结果")))
        return 1;

    QTemporaryDir temporary;
    if (!check(temporary.isValid(), QStringLiteral("无法创建临时目录")))
        return 1;

    const QString inputDirectory = QDir(temporary.path()).absoluteFilePath(QStringLiteral("实拍图输入"));
    if (!check(QDir().mkpath(inputDirectory), QStringLiteral("无法创建中文输入目录")))
        return 1;
    const QString imagePath    = QDir(inputDirectory).absoluteFilePath(QStringLiteral("photo.jpg"));
    const QString databasePath = QDir(inputDirectory).absoluteFilePath(QStringLiteral("gsm.db"));
    QFile         image(imagePath);
    if (!check(image.open(QIODevice::WriteOnly) && image.write("photo-content") > 0, QStringLiteral("无法创建测试图片")))
        return 1;
    image.close();

    StoredMatchResult result;
    result.upper = {QStringLiteral("TOP001"), QStringLiteral("top.png"), false};
    result.lower = {QStringLiteral("BOTTOM002"), QStringLiteral("bottom.png"), false};
    QString error;
    if (!check(MatchResultStore::save(databasePath, imagePath, result, &error), QStringLiteral("保存未确认匹配结果失败: %1").arg(error)))
        return 1;

    auto loaded = MatchResultStore::load(databasePath, imagePath, &error);
    if (!check(loaded && loaded->photoFileName == QStringLiteral("photo.jpg") && loaded->photoFileSize == QFileInfo(imagePath).size() &&
                   loaded->photoMd5.size() == 32 && loaded->upper.styleId == QStringLiteral("TOP001") && !loaded->upper.confirmed &&
                   loaded->lower.imageName == QStringLiteral("bottom.png") && !loaded->lower.confirmed,
               QStringLiteral("数据库应按文件名、大小和 MD5 恢复两个未确认结果: %1").arg(error)))
        return 1;

    result.upper.confirmed = true;
    result.lower           = {};
    if (!check(MatchResultStore::save(databasePath, imagePath, result, &error), QStringLiteral("更新确认/删除状态失败: %1").arg(error)))
        return 1;
    loaded = MatchResultStore::load(databasePath, imagePath, &error);
    if (!check(loaded && loaded->upper.confirmed && loaded->lower.isEmpty(), QStringLiteral("应保留已确认上衣并删除错误的裤裙匹配")))
        return 1;

    if (!check(image.open(QIODevice::Append) && image.write("-changed") > 0, QStringLiteral("无法修改测试图片内容")))
        return 1;
    image.close();
    if (!check(!MatchResultStore::load(databasePath, imagePath, &error) && error.isEmpty(),
               QStringLiteral("文件内容变化后 MD5 不同，不应误用旧匹配记录")))
        return 1;

    if (!check(image.open(QIODevice::WriteOnly | QIODevice::Truncate) && image.write("photo-content") > 0, QStringLiteral("无法恢复测试图片内容")))
        return 1;
    image.close();

    result.upper = {};
    if (!check(MatchResultStore::save(databasePath, imagePath, result, &error), QStringLiteral("删除空匹配记录失败: %1").arg(error)))
        return 1;
    if (!check(!MatchResultStore::load(databasePath, imagePath, &error), QStringLiteral("两个部位均删除后不应保留数据库记录")))
        return 1;

    return 0;
}
