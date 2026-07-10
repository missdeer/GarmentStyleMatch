#pragma once

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
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedIndexChanged)

public:
    enum Roles
    {
        StyleIdRole = Qt::UserRole + 1,
        ImagePathRole,
        TagRole,
        IndexLabelRole,
        SelectedRole,
    };

    explicit GalleryListModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant               data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void               setItems(QVector<GalleryItem> items);
    void               loadFromStyleCacheDir(const QString &directoryPath);
    void               setFilterText(const QString &text);
    QString            filterText() const { return m_filterText; }
    const GalleryItem *at(int row) const;
    int                selectedIndex() const
    {
        return m_selectedIndex;
    }

    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void selectedIndexChanged();

private:
    void rebuildFilteredItems();

    QVector<GalleryItem> m_allItems;
    QVector<GalleryItem> m_items;
    QString              m_filterText;
    int                  m_selectedIndex = -1;
};
