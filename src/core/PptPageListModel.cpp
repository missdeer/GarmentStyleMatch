#include "PptPageListModel.h"

PptPageListModel::PptPageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PptPageListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant PptPageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const PptPageItem &it = m_items.at(index.row());
    switch (role) {
    case PageIndexRole: return it.pageIndex;
    case ImagePathRole: return it.imagePath;
    case SelectedRole:  return it.selected;
    default: return {};
    }
}

QHash<int, QByteArray> PptPageListModel::roleNames() const
{
    return {
        {PageIndexRole, "pageIndex"},
        {ImagePathRole, "imagePath"},
        {SelectedRole,  "selected"},
    };
}

void PptPageListModel::setItems(QVector<PptPageItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    emit countChanged();
    emit selectedCountChanged();
}

const PptPageItem *PptPageListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

QVector<int> PptPageListModel::selectedRows() const
{
    QVector<int> rows;
    rows.reserve(m_items.size());
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).selected)
            rows.push_back(i);
    return rows;
}

int PptPageListModel::selectedCount() const
{
    int n = 0;
    for (const auto &it : m_items)
        if (it.selected) ++n;
    return n;
}

void PptPageListModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_items[row].selected = !m_items[row].selected;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {SelectedRole});
    emit selectedCountChanged();
}

void PptPageListModel::setSelected(int row, bool on)
{
    if (row < 0 || row >= m_items.size())
        return;
    if (m_items[row].selected == on)
        return;
    m_items[row].selected = on;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {SelectedRole});
    emit selectedCountChanged();
}

void PptPageListModel::clearSelection()
{
    bool any = false;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].selected) {
            m_items[i].selected = false;
            any = true;
        }
    }
    if (!any)
        return;
    emit dataChanged(index(0), index(m_items.size() - 1), {SelectedRole});
    emit selectedCountChanged();
}

void PptPageListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
    emit selectedCountChanged();
}
