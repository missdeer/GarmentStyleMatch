#include <memory>
#include <optional>
#include <utility>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QHash>

#include "GalleryListModel.h"
#include "LuaCategoryRuleEngine.h"
#include "SQLiteDB.h"
#include "SQLiteStatement.h"

namespace
{
    QString normalizedStyleId(const QString &styleId)
    {
        return styleId.trimmed().toUpper();
    }

    QString partName(LuaCategoryRuleEngine::Part part)
    {
        using Part = LuaCategoryRuleEngine::Part;
        switch (part)
        {
        case Part::Upper:
            return QStringLiteral("upper");
        case Part::Lower:
            return QStringLiteral("lower");
        case Part::Accessory:
            return QStringLiteral("accessory");
        case Part::Dress:
            return QStringLiteral("dress");
        case Part::Unknown:
            return QStringLiteral("unknown");
        }
        return QStringLiteral("unknown");
    }

    struct CoarseClassification
    {
        QString part = QStringLiteral("unknown");
        QString categoryCode;
        QString error;
    };

    CoarseClassification coarseClassification(const LuaCategoryRuleEngine::Result &result)
    {
        return {partName(result.part), result.categoryCode, result.error};
    }

    bool isValidClassification(const CoarseClassification &classification)
    {
        return classification.error.isEmpty() && (classification.part == QLatin1String("upper") || classification.part == QLatin1String("lower") ||
                                                  classification.part == QLatin1String("accessory") ||
                                                  classification.part == QLatin1String("dress") || classification.part == QLatin1String("unknown"));
    }

    bool prepareCategoryCache(SQLiteDB &database)
    {
        if (!database.execute("CREATE TABLE IF NOT EXISTS gallery_categories("
                              "normalized_style_id TEXT NOT NULL,rule_id TEXT NOT NULL,rule_version TEXT NOT NULL,rule_sha256 TEXT NOT NULL,"
                              "part TEXT NOT NULL,category_code TEXT NOT NULL DEFAULT '',"
                              "PRIMARY KEY(normalized_style_id,rule_id,rule_version,rule_sha256))"))
        {
            return false;
        }

        SQLiteStatement columns = database.prepare("PRAGMA table_info(gallery_categories)");
        if (!columns)
        {
            return false;
        }
        bool                        hasCategoryCode = false;
        SQLiteStatement::StepResult result;
        while ((result = columns.step()) == SQLiteStatement::StepResult::Row)
        {
            if (columns.columnText(1) == QLatin1String("category_code"))
            {
                hasCategoryCode = true;
            }
        }
        return result == SQLiteStatement::StepResult::Done &&
               (hasCategoryCode || database.execute("ALTER TABLE gallery_categories ADD COLUMN category_code TEXT NOT NULL DEFAULT ''"));
    }

    std::optional<CoarseClassification> loadCachedCategory(
        SQLiteDB &database, const QString &styleId, const QString &ruleId, const QString &ruleVersion, const QString &ruleSha256)
    {
        SQLiteStatement statement = database.prepare("SELECT part,category_code "
                                                     "FROM gallery_categories WHERE normalized_style_id=?1 AND rule_id=?2 AND rule_version=?3 "
                                                     "AND rule_sha256=?4");
        if (!statement || !statement.bindText(1, styleId) || !statement.bindText(2, ruleId) || !statement.bindText(3, ruleVersion) ||
            !statement.bindText(4, ruleSha256) || statement.step() != SQLiteStatement::StepResult::Row)
        {
            return std::nullopt;
        }

        CoarseClassification classification {statement.columnText(0), statement.columnText(1), {}};
        return isValidClassification(classification) ? std::optional<CoarseClassification>(std::move(classification)) : std::nullopt;
    }

    void saveCachedCategory(SQLiteDB                   &database,
                            const QString              &styleId,
                            const QString              &ruleId,
                            const QString              &ruleVersion,
                            const QString              &ruleSha256,
                            const CoarseClassification &classification)
    {
        SQLiteStatement statement =
            database.prepare("INSERT INTO gallery_categories(normalized_style_id,rule_id,rule_version,rule_sha256,part,category_code) "
                             "VALUES(?1,?2,?3,?4,?5,?6) ON CONFLICT(normalized_style_id,rule_id,rule_version,rule_sha256) "
                             "DO UPDATE SET part=excluded.part,category_code=excluded.category_code");
        if (!statement || !statement.bindText(1, styleId) || !statement.bindText(2, ruleId) || !statement.bindText(3, ruleVersion) ||
            !statement.bindText(4, ruleSha256) || !statement.bindText(5, classification.part) || !statement.bindText(6, classification.categoryCode))
        {
            return;
        }
        (void)statement.step();
    }
} // namespace

GalleryListModel::GalleryListModel(QObject *parent) : QAbstractListModel(parent) {}

GalleryListModel::~GalleryListModel() = default;

int GalleryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant GalleryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    {
        return {};
    }

    const GalleryItem &item = m_items.at(index.row());
    switch (role)
    {
    case StyleIdRole:
        return item.styleId;
    case ImagePathRole:
        return item.imagePath;
    case TagRole:
        return item.tag;
    case IndexLabelRole:
        return index.row() + 1;
    case PartRole:
        return item.part;
    case CategoryErrorRole:
        return item.categoryError;
    default:
        return {};
    }
}

QHash<int, QByteArray> GalleryListModel::roleNames() const
{
    return {
        {StyleIdRole, "styleId"},
        {ImagePathRole, "imagePath"},
        {TagRole, "tag"},
        {IndexLabelRole, "indexLabel"},
        {PartRole, "part"},
        {CategoryErrorRole, "categoryError"},
    };
}

void GalleryListModel::setItems(QVector<GalleryItem> items)
{
    classifyItems(items);
    beginResetModel();
    m_allItems.swap(items);
    rebuildFilteredItems();
    endResetModel();
    emit countChanged();
    emit classificationChanged();
}

void GalleryListModel::setCategoryCachePath(const QString &databasePath)
{
    if (databasePath == m_categoryCachePath)
    {
        return;
    }
    m_categoryCachePath = databasePath;
    if (!m_allItems.isEmpty())
    {
        beginResetModel();
        classifyItems(m_allItems);
        rebuildFilteredItems();
        endResetModel();
    }
}

void GalleryListModel::setCategoryRuleScript(const QByteArray &script, bool forceReload)
{
    if (!forceReload && script == m_categoryRuleScript)
    {
        return;
    }

    m_categoryRuleScript = script;
    m_categoryRule.reset();
    m_categoryRuleSha256.clear();
    if (!script.isEmpty())
    {
        m_categoryRuleSha256 = QString::fromLatin1(QCryptographicHash::hash(script, QCryptographicHash::Sha256).toHex());
        m_categoryRule       = std::make_unique<LuaCategoryRuleEngine>(script);
    }

    if (!m_allItems.isEmpty())
    {
        beginResetModel();
        classifyItems(m_allItems);
        rebuildFilteredItems();
        endResetModel();
    }
    emit classificationChanged();
}

bool GalleryListModel::categoryRuleReady() const
{
    return m_categoryRule && m_categoryRule->state() == LuaCategoryRuleEngine::State::Ready;
}

QString GalleryListModel::categoryRuleId() const
{
    return m_categoryRule ? m_categoryRule->ruleId() : QString();
}

QString GalleryListModel::categoryRuleError() const
{
    return m_categoryRule && m_categoryRule->state() != LuaCategoryRuleEngine::State::Ready ? m_categoryRule->errorMessage() : QString();
}

void GalleryListModel::classifyItems(QVector<GalleryItem> &items) const
{
    for (GalleryItem &item : items)
    {
        item.part = QStringLiteral("unknown");
        item.categoryError.clear();
        item.categoryCode.clear();
    }
    if (!m_categoryRule)
    {
        return;
    }
    if (m_categoryRule->state() != LuaCategoryRuleEngine::State::Ready)
    {
        const QString error = m_categoryRule->errorMessage();
        for (GalleryItem &item : items)
        {
            item.categoryError = error;
        }
        return;
    }

    std::unique_ptr<SQLiteDB> database;
    if (!m_categoryCachePath.isEmpty())
    {
        QDir().mkpath(QFileInfo(m_categoryCachePath).absolutePath());
        auto candidate = std::make_unique<SQLiteDB>(m_categoryCachePath);
        if (*candidate && prepareCategoryCache(*candidate))
        {
            database = std::move(candidate);
        }
    }

    const QString                        ruleId                  = m_categoryRule->ruleId();
    const QString                        ruleVersion             = m_categoryRule->ruleVersion();
    const bool                           cacheTransactionStarted = database && database->execute("BEGIN TRANSACTION");
    QHash<QString, CoarseClassification> classifications;
    classifications.reserve(items.size());
    for (GalleryItem &item : items)
    {
        const QString styleId = normalizedStyleId(item.styleId);
        auto          found   = classifications.constFind(styleId);
        if (found == classifications.constEnd())
        {
            std::optional<CoarseClassification> classification;
            if (database)
            {
                classification = loadCachedCategory(*database, styleId, ruleId, ruleVersion, m_categoryRuleSha256);
            }
            if (!classification)
            {
                classification = coarseClassification(m_categoryRule->classify(styleId));
                if (cacheTransactionStarted && isValidClassification(*classification))
                {
                    saveCachedCategory(*database, styleId, ruleId, ruleVersion, m_categoryRuleSha256, *classification);
                }
            }
            found = classifications.constFind(classifications.insert(styleId, std::move(*classification)).key());
        }
        item.part          = found->part;
        item.categoryError = found->error;
        item.categoryCode  = found->categoryCode;
    }
    if (cacheTransactionStarted)
    {
        (void)database->execute("COMMIT");
    }
}

void GalleryListModel::setFilterText(const QString &text)
{
    const QString normalizedText = text.trimmed();
    if (normalizedText == m_filterText)
    {
        return;
    }

    beginResetModel();
    m_filterText = normalizedText;
    rebuildFilteredItems();
    endResetModel();
    emit countChanged();
}

void GalleryListModel::rebuildFilteredItems()
{
    m_items.clear();
    if (m_filterText.isEmpty())
    {
        m_items = m_allItems;
        return;
    }

    for (const GalleryItem &item : std::as_const(m_allItems))
    {
        if (item.styleId.contains(m_filterText, Qt::CaseInsensitive))
        {
            m_items.push_back(item);
        }
    }
}

void GalleryListModel::loadFromStyleCacheDir(const QString &directoryPath)
{
    QVector<GalleryItem> items;
    if (directoryPath.isEmpty() || !QDir(directoryPath).exists())
    {
        setItems(std::move(items));
        return;
    }

    const QDir root(directoryPath);
    const auto styleDirectories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &styleDirectoryInfo : styleDirectories)
    {
        const QString styleId = styleDirectoryInfo.fileName();
        const QDir    styleDirectory(styleDirectoryInfo.absoluteFilePath());
        const auto    imageFiles = styleDirectory.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &imageFile : imageFiles)
        {
            GalleryItem item;
            item.styleId   = styleId;
            item.imagePath = imageFile.absoluteFilePath();
            item.tag       = QStringLiteral("baby");
            items.push_back(std::move(item));
        }
    }
    setItems(std::move(items));
}

const GalleryItem *GalleryListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
    {
        return nullptr;
    }
    return &m_items.at(row);
}

void GalleryListModel::clear()
{
    if (m_allItems.isEmpty())
    {
        return;
    }
    beginResetModel();
    m_allItems.clear();
    m_items.clear();
    endResetModel();
    emit countChanged();
    emit classificationChanged();
}
