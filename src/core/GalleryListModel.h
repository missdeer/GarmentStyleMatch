#pragma once

#include <cstdint>
#include <memory>

#include <QAbstractListModel>
#include <QByteArray>
#include <QString>
#include <QVector>

class LuaCategoryRuleEngine;

struct GalleryItem
{
    QString styleId;   // e.g. "T0JE26B38A008"
    QString imagePath; // reference sketch/gallery image path
    QString tag;       // e.g. "baby"
    QString part = QStringLiteral("unknown");
    QString categoryError;
    QString categoryCode;
};

class GalleryListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles : std::uint16_t
    {
        StyleIdRole = Qt::UserRole + 1,
        ImagePathRole,
        TagRole,
        IndexLabelRole,
        PartRole,
        CategoryErrorRole,
    };

    explicit GalleryListModel(QObject *parent = nullptr);
    ~GalleryListModel() override;

    [[nodiscard]] int                    rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant               data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void                  setItems(QVector<GalleryItem> items);
    void                  loadFromStyleCacheDir(const QString &directoryPath);
    void                  setCategoryCachePath(const QString &databasePath);
    void                  setCategoryRuleScript(const QByteArray &script, bool forceReload = false);
    [[nodiscard]] bool    categoryRuleReady() const;
    [[nodiscard]] QString categoryRuleId() const;
    [[nodiscard]] QString categoryRuleError() const;
    void                  setFilterText(const QString &text);
    [[nodiscard]] QString filterText() const
    {
        return m_filterText;
    }
    [[nodiscard]] const GalleryItem          *at(int row) const;
    [[nodiscard]] const QVector<GalleryItem> &allItems() const
    {
        return m_allItems;
    }
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void classificationChanged();

private:
    void classifyItems(QVector<GalleryItem> &items) const;
    void rebuildFilteredItems();

    QVector<GalleryItem>                   m_allItems;
    QVector<GalleryItem>                   m_items;
    QString                                m_filterText;
    QString                                m_categoryCachePath;
    QByteArray                             m_categoryRuleScript;
    QString                                m_categoryRuleSha256;
    std::unique_ptr<LuaCategoryRuleEngine> m_categoryRule;
};
