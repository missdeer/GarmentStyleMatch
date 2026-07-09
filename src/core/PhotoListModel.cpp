#include "PhotoListModel.h"

#include <QDir>

PhotoListModel::PhotoListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PhotoListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant PhotoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const PhotoItem &it = m_items.at(index.row());
    switch (role) {
    case FileNameRole:  return it.fileName;
    case ImagePathRole: return it.imagePath;
    case ProcessedRole: return it.processed;
    case DisplayLineRole:
        return QStringLiteral("%1  %2")
            .arg(it.processed ? QStringLiteral("\xE2\x9C\x93") : QStringLiteral(" "),
                 QDir::toNativeSeparators(it.imagePath));
    default: return {};
    }
}

QHash<int, QByteArray> PhotoListModel::roleNames() const
{
    return {
        {FileNameRole,    "fileName"},
        {ImagePathRole,   "imagePath"},
        {ProcessedRole,   "processed"},
        {DisplayLineRole, "displayLine"},
    };
}

void PhotoListModel::setItems(QVector<PhotoItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    emit countChanged();
}

const PhotoItem *PhotoListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

void PhotoListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::markProcessed(int row, bool processed)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_items[row].processed = processed;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ProcessedRole, DisplayLineRole});
}
