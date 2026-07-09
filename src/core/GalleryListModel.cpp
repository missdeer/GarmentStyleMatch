#include "GalleryListModel.h"

GalleryListModel::GalleryListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GalleryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant GalleryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const GalleryItem &it = m_items.at(index.row());
    switch (role) {
    case StyleIdRole:    return it.styleId;
    case ImagePathRole:  return it.imagePath;
    case TagRole:        return it.tag;
    case IndexLabelRole: return index.row() + 1;
    default: return {};
    }
}

QHash<int, QByteArray> GalleryListModel::roleNames() const
{
    return {
        {StyleIdRole,    "styleId"},
        {ImagePathRole,  "imagePath"},
        {TagRole,        "tag"},
        {IndexLabelRole, "indexLabel"},
    };
}

void GalleryListModel::setItems(QVector<GalleryItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    emit countChanged();
}

const GalleryItem *GalleryListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

void GalleryListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}
