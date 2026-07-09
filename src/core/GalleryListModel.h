#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QString>

struct GalleryItem
{
    QString styleId;    // e.g. "T0JE26B38A008"
    QString imagePath;  // reference sketch/gallery image path
    QString tag;        // e.g. "baby"
};

class GalleryListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        StyleIdRole = Qt::UserRole + 1,
        ImagePathRole,
        TagRole,
        IndexLabelRole,
    };

    explicit GalleryListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<GalleryItem> items);
    const GalleryItem *at(int row) const;

    Q_INVOKABLE void clear();

signals:
    void countChanged();

private:
    QVector<GalleryItem> m_items;
};
