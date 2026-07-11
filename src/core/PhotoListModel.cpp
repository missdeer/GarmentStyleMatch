#include <utility>

#include <QDir>

#include "PhotoListModel.h"

PhotoListModel::PhotoListModel(QObject *parent) : QAbstractListModel(parent) {}

int PhotoListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_visibleRows.size());
}

QVariant PhotoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size())
    {
        return {};
    }

    const PhotoItem &item = m_items.at(m_visibleRows.at(index.row()));
    switch (role)
    {
    case FileNameRole:
        return item.fileName;
    case ImagePathRole:
        return item.imagePath;
    case ProcessedRole:
        return item.processed;
    case DisplayLineRole:
        return QDir::toNativeSeparators(item.relativePath.isEmpty() ? item.fileName : item.relativePath);
    default:
        return {};
    }
}

QHash<int, QByteArray> PhotoListModel::roleNames() const
{
    return {
        {FileNameRole, "fileName"},
        {ImagePathRole, "imagePath"},
        {ProcessedRole, "processed"},
        {DisplayLineRole, "displayLine"},
    };
}

void PhotoListModel::setItems(QVector<PhotoItem> items)
{
    beginResetModel();
    m_items.swap(items);
    rebuildVisibleRows();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::setFilterText(const QString &text)
{
    const QString normalizedText = text.trimmed();
    if (normalizedText == m_filterText)
    {
        return;
    }
    beginResetModel();
    m_filterText = normalizedText;
    rebuildVisibleRows();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    for (int row = 0; row < m_items.size(); ++row)
    {
        const PhotoItem &item = m_items.at(row);
        if (m_filterText.isEmpty() || item.fileName.contains(m_filterText, Qt::CaseInsensitive) ||
            item.relativePath.contains(m_filterText, Qt::CaseInsensitive) || item.imagePath.contains(m_filterText, Qt::CaseInsensitive))
        {
            m_visibleRows.push_back(row);
        }
    }
}

const PhotoItem *PhotoListModel::at(int row) const
{
    if (row < 0 || row >= m_visibleRows.size())
    {
        return nullptr;
    }
    return &m_items.at(m_visibleRows.at(row));
}

void PhotoListModel::clear()
{
    if (m_items.isEmpty())
    {
        return;
    }
    beginResetModel();
    m_items.clear();
    m_visibleRows.clear();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::markProcessed(int row, bool processed)
{
    if (row < 0 || row >= m_visibleRows.size())
    {
        return;
    }
    const int itemIndex = m_visibleRows.at(row);
    PhotoItem item      = m_items.at(itemIndex);
    item.processed      = processed;
    m_items.replace(itemIndex, std::move(item));
    const QModelIndex idx = index(row);
    emit              dataChanged(idx, idx, {ProcessedRole, DisplayLineRole});
}
