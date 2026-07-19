#pragma once

#include <cstdint>

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct GalleryItem
{
    QString styleId;   // e.g. "T0JE26B38A008"
    QString imagePath; // reference sketch/gallery image path
    QString tag;       // e.g. "baby"
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
    };

    explicit GalleryListModel(QObject *parent = nullptr);

    [[nodiscard]] int                    rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant               data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void                  setItems(QVector<GalleryItem> items);
    void                  loadFromStyleCacheDir(const QString &directoryPath);
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

private:
    void rebuildFilteredItems();

    QVector<GalleryItem> m_allItems;
    QVector<GalleryItem> m_items;
    QString              m_filterText;
};
