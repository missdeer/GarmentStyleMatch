#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "GalleryListModel.h"
#include "SQLiteDB.h"

namespace
{
    constexpr qint64 kImagePayloadSize = 5;

    const QByteArray kUpperRule = QByteArrayLiteral(R"lua(
local calls = {}
local function unknown()
    return { recognized = false, part = "unknown" }
end
local function classify(input)
    if input == "T0JE26B38A008" or input == "T0ZZ26B38A008" then
        calls[input] = (calls[input] or 0) + 1
        if calls[input] > 1 then
            error("style classified more than once")
        end
    end
    if input == "T0JE26B38A008" then
        return {
            recognized = true, categoryCode = "JE", level1Code = "2", level1Name = "外套",
            level2Code = "2.8", level2Name = "牛仔外套", part = "upper"
        }
    end
    if input == "T0ZZ26B38A008" then
        return { recognized = false, categoryCode = "ZZ", part = "unknown" }
    end
    return unknown()
end
return {
    ruleId = "cache-test", version = "1", classify = classify
}
)lua");

    const QByteArray kChangedContentRule = QByteArrayLiteral(R"lua(
local function unknown()
    return { recognized = false, part = "unknown" }
end
local function classify(input)
    if input == "T0JE26B38A008" then
        return {
            recognized = true, categoryCode = "JE", level1Code = "3", level1Name = "裤子",
            level2Code = "3.3", level2Name = "牛仔裤", part = "lower"
        }
    end
    return unknown()
end
return {
    ruleId = "cache-test", version = "1", classify = classify
}
)lua");

    const QByteArray kTransientFailureRule = QByteArrayLiteral(R"lua(
local attempts = {}
local function unknown()
    return { recognized = false, part = "unknown" }
end
local function known()
    return {
        recognized = true, categoryCode = "JE", level1Code = "2", level1Name = "外套",
        level2Code = "2.8", level2Name = "牛仔外套", part = "upper"
    }
end
local function classify(input)
    if input == "T0JE26B38A008" then
        attempts[input] = (attempts[input] or 0) + 1
        if attempts[input] == 1 then
            error("transient classification failure")
        end
        return known()
    end
    return unknown()
end
return {
    ruleId = "retry-test", version = "1", classify = classify
}
)lua");

    bool check(bool condition, const QString &message)
    {
        if (!condition)
        {
            qCritical().noquote() << message;
        }
        return condition;
    }

    bool writeFile(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("image") == kImagePayloadSize;
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

    if (!check(!model.roleNames().values().contains(QByteArrayLiteral("selected")), QStringLiteral("款号小图库模型不得再暴露列表项选择状态")))
    {
        return 1;
    }

    model.setFilterText(QStringLiteral("style2"));
    if (!check(model.rowCount() == 1 && model.at(0)->styleId == QStringLiteral("STYLE200B"),
               QStringLiteral("搜索应即时按款号进行不区分大小写的包含匹配")))
    {
        return 1;
    }

    model.setFilterText(QStringLiteral("missing"));
    if (!check(model.rowCount() == 0, QStringLiteral("无匹配款号时图库应为空")))
    {
        return 1;
    }
    if (!check(model.allItems().size() == 3, QStringLiteral("自动匹配必须使用完整图库，不能被界面搜索过滤缩小范围")))
    {
        return 1;
    }

    model.setFilterText(QStringLiteral("  "));
    if (!check(model.rowCount() == 3, QStringLiteral("清空搜索内容后应恢复全部款号")))
    {
        return 1;
    }

    QTemporaryDir temporary;
    if (!check(temporary.isValid(), QStringLiteral("无法创建缓存加载测试目录")))
    {
        return 1;
    }
    const QString stylesDir = temporary.filePath(QStringLiteral("styles"));
    if (!check(writeFile(QDir(stylesDir).filePath(QStringLiteral("STYLE100A/001.png"))) &&
                   writeFile(QDir(stylesDir).filePath(QStringLiteral("STYLE100A/002.png"))) &&
                   writeFile(QDir(stylesDir).filePath(QStringLiteral("STYLE200B/001.jpg"))) &&
                   writeFile(QDir(stylesDir).filePath(QStringLiteral("LEGACY300C.png"))),
               QStringLiteral("无法创建款式缓存测试数据")))
    {
        return 1;
    }

    model.loadFromStyleCacheDir(stylesDir);
    if (!check(model.rowCount() == 3, QStringLiteral("只应加载款号子目录中的 3 张图片，styles 根目录图片必须忽略")))
    {
        return 1;
    }
    if (!check(model.at(0)->styleId == QStringLiteral("STYLE100A") && model.at(1)->styleId == QStringLiteral("STYLE100A") &&
                   model.at(2)->styleId == QStringLiteral("STYLE200B"),
               QStringLiteral("缓存图片必须使用父目录名作为款号")))
    {
        return 1;
    }

    const QString categoryDatabasePath = temporary.filePath(QStringLiteral("category-cache.sqlite"));
    const QString embeddingCachePath   = temporary.filePath(QStringLiteral("style-embeddings.sqlite"));
    if (!check(writeFile(embeddingCachePath), QStringLiteral("无法创建独立图片特征缓存哨兵")))
    {
        return 1;
    }
    QFile embeddingCache(embeddingCachePath);
    if (!check(embeddingCache.open(QIODevice::ReadOnly), QStringLiteral("无法读取图片特征缓存哨兵")))
    {
        return 1;
    }
    const QByteArray embeddingCacheBefore = embeddingCache.readAll();
    embeddingCache.close();

    {
        SQLiteDB legacyCategoryDatabase(categoryDatabasePath);
        if (!check(legacyCategoryDatabase &&
                       legacyCategoryDatabase.execute("CREATE TABLE gallery_categories("
                                                      "normalized_style_id TEXT NOT NULL,rule_id TEXT NOT NULL,rule_version TEXT NOT NULL,"
                                                      "rule_sha256 TEXT NOT NULL,part TEXT NOT NULL,"
                                                      "PRIMARY KEY(normalized_style_id,rule_id,rule_version,rule_sha256))"),
                   QStringLiteral("无法准备旧版分类缓存迁移测试数据库")))
        {
            return 1;
        }
    }

    GalleryListModel classifiedModel;
    classifiedModel.setCategoryCachePath(categoryDatabasePath);
    classifiedModel.setCategoryRuleScript(kUpperRule);
    classifiedModel.setItems({
        {QStringLiteral(" t0je26b38a008 "), QStringLiteral("first.png"), QStringLiteral("baby")},
        {QStringLiteral("T0JE26B38A008"), QStringLiteral("second.png"), QStringLiteral("baby")},
        {QStringLiteral("T0ZZ26B38A008"), QStringLiteral("unknown.png"), QStringLiteral("baby")},
    });
    const auto &classifiedItems = classifiedModel.allItems();
    if (!check(classifiedItems.size() == 3 && classifiedItems.at(0).part == QStringLiteral("upper") &&
                   classifiedItems.at(1).part == QStringLiteral("upper") && classifiedItems.at(2).part == QStringLiteral("unknown") &&
                   classifiedItems.at(2).categoryCode == QStringLiteral("ZZ") && classifiedItems.at(2).categoryError.isEmpty(),
               QStringLiteral("规范化后的同款多图必须只分类一次并传播一致结果，未知代码必须迁移旧缓存并保留诊断代码")))
    {
        return 1;
    }
    if (!check(classifiedModel.roleNames().value(GalleryListModel::PartRole) == QByteArrayLiteral("part") &&
                   classifiedModel.data(classifiedModel.index(0), GalleryListModel::PartRole).toString() == QStringLiteral("upper") &&
                   !classifiedModel.roleNames().values().contains(QByteArrayLiteral("categoryCode")) &&
                   !classifiedModel.roleNames().values().contains(QByteArrayLiteral("level1Name")) &&
                   !classifiedModel.roleNames().values().contains(QByteArrayLiteral("level2Name")),
               QStringLiteral("图库模型只能向下游暴露 JIRA 定义的粗分类 part，不能携带品牌一二级品类信息")))
    {
        return 1;
    }

    classifiedModel.setItems({
        {QStringLiteral("T0JE26B38A008"), QStringLiteral("first.png"), QStringLiteral("baby")},
        {QStringLiteral("T0JE26B38A008"), QStringLiteral("second.png"), QStringLiteral("baby")},
        {QStringLiteral("T0ZZ26B38A008"), QStringLiteral("unknown.png"), QStringLiteral("baby")},
    });
    if (!check(classifiedModel.allItems().at(0).part == QStringLiteral("upper") && classifiedModel.allItems().at(1).part == QStringLiteral("upper") &&
                   classifiedModel.allItems().at(2).part == QStringLiteral("unknown") && classifiedModel.allItems().at(2).categoryError.isEmpty(),
               QStringLiteral("规则上下文未变化时，已识别结果和业务 unknown 都必须复用，不能再次执行规则")) ||
        !check(classifiedModel.allItems().at(2).categoryCode == QStringLiteral("ZZ"),
               QStringLiteral("缓存复用业务 unknown 时必须同时恢复未知品类代码")))
    {
        return 1;
    }

    classifiedModel.setCategoryRuleScript(kChangedContentRule);
    if (!check(classifiedModel.allItems().at(0).part == QStringLiteral("lower") && classifiedModel.allItems().at(1).part == QStringLiteral("lower") &&
                   classifiedModel.allItems().at(0).imagePath == QStringLiteral("first.png"),
               QStringLiteral("规则内容改变必须立即重分类已有去重款号，但不得重新导入或丢弃图库图片")))
    {
        return 1;
    }
    if (!check(embeddingCache.open(QIODevice::ReadOnly) && embeddingCache.readAll() == embeddingCacheBefore,
               QStringLiteral("规则内容改变只能失效分类缓存，不能改动独立的图片特征缓存")))
    {
        return 1;
    }
    embeddingCache.close();

    classifiedModel.setCategoryRuleScript({});
    if (!check(classifiedModel.allItems().at(0).part == QStringLiteral("unknown") && classifiedModel.allItems().at(0).categoryError.isEmpty(),
               QStringLiteral("没有活跃规则时图库必须保留并安全显示为 unknown")))
    {
        return 1;
    }
    classifiedModel.setCategoryRuleScript(kUpperRule);
    if (!check(classifiedModel.allItems().at(0).part == QStringLiteral("upper"),
               QStringLiteral("从无规则首次启用有效规则时必须重分类已有图库，无需重新导入图片")))
    {
        return 1;
    }

    GalleryListModel retryModel;
    retryModel.setCategoryCachePath(categoryDatabasePath);
    retryModel.setCategoryRuleScript(kTransientFailureRule);
    retryModel.setItems({{QStringLiteral("T0JE26B38A008"), QStringLiteral("retry.png"), QStringLiteral("baby")}});
    if (!check(retryModel.allItems().at(0).part == QStringLiteral("unknown") && !retryModel.allItems().at(0).categoryError.isEmpty(),
               QStringLiteral("单项规则执行失败必须安全回退为 unknown，并保留失败原因")))
    {
        return 1;
    }
    retryModel.setItems({{QStringLiteral("T0JE26B38A008"), QStringLiteral("retry.png"), QStringLiteral("baby")}});
    if (!check(retryModel.allItems().at(0).part == QStringLiteral("upper") && retryModel.allItems().at(0).categoryError.isEmpty(),
               QStringLiteral("故障回退 unknown 不得持久复用，后续导入必须重试并可恢复")))
    {
        return 1;
    }

    return 0;
}
