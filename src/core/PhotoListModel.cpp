#include "PhotoListModel.h"

#include <QDir>

#include <utility>

PhotoListModel::PhotoListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PhotoListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visibleRows.size();
}

QVariant PhotoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size())
        return {};

    const PhotoItem &it = m_items.at(m_visibleRows.at(index.row()));
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
    rebuildVisibleRows();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::setFilterText(const QString &text)
{
    const QString normalizedText = text.trimmed();
    if (normalizedText == m_filterText)
        return;
    beginResetModel();
    m_filterText = normalizedText;
    rebuildVisibleRows();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    for (int row = 0; row < m_items.size(); ++row) {
        const PhotoItem &item = m_items.at(row);
        if (m_filterText.isEmpty()
            || item.fileName.contains(m_filterText, Qt::CaseInsensitive)
            || item.imagePath.contains(m_filterText, Qt::CaseInsensitive))
            m_visibleRows.push_back(row);
    }
}

const PhotoItem *PhotoListModel::at(int row) const
{
    if (row < 0 || row >= m_visibleRows.size())
        return nullptr;
    return &m_items.at(m_visibleRows.at(row));
}

void PhotoListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    m_visibleRows.clear();
    endResetModel();
    emit countChanged();
}

void PhotoListModel::markProcessed(int row, bool processed)
{
    if (row < 0 || row >= m_visibleRows.size())
        return;
    m_items[m_visibleRows.at(row)].processed = processed;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ProcessedRole, DisplayLineRole});
}
