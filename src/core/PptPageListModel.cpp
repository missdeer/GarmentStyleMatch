#include <algorithm>

#include <QStringList>

#include "PptPageListModel.h"

PptPageListModel::PptPageListModel(QObject *parent) : QAbstractListModel(parent) {}

int PptPageListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant PptPageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    {
        return {};
    }

    const PptPageItem &item = m_items.at(index.row());
    switch (role)
    {
    case PageIndexRole:
        return item.pageIndex;
    case ImagePathRole:
        return item.imagePath;
    case SelectedRole:
        return item.selected;
    default:
        return {};
    }
}

QHash<int, QByteArray> PptPageListModel::roleNames() const
{
    return {
        {PageIndexRole, "pageIndex"},
        {ImagePathRole, "imagePath"},
        {SelectedRole, "selected"},
    };
}

void PptPageListModel::setItems(QVector<PptPageItem> items)
{
    for (PptPageItem &item : items)
    {
        if (item.selected)
        {
            m_selectedPages.insert(item.pageIndex);
        }
        else
        {
            item.selected = m_selectedPages.contains(item.pageIndex);
        }
    }
    beginResetModel();
    m_items.swap(items);
    endResetModel();
    emit countChanged();
    emit selectedCountChanged();
    emit selectedPagesTextChanged();
}

void PptPageListModel::appendItem(const PptPageItem &item)
{
    PptPageItem storedItem = item;
    if (storedItem.selected)
    {
        m_selectedPages.insert(storedItem.pageIndex);
    }
    else
    {
        storedItem.selected = m_selectedPages.contains(storedItem.pageIndex);
    }
    const int row = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    m_items.push_back(std::move(storedItem));
    endInsertRows();
    emit countChanged();
    if (m_items.back().selected)
    {
        emit selectedCountChanged();
    }
}

const PptPageItem *PptPageListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
    {
        return nullptr;
    }
    return &m_items.at(row);
}

QVector<int> PptPageListModel::selectedRows() const
{
    QVector<int> rows;
    rows.reserve(m_items.size());
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (m_items.at(i).selected)
        {
            rows.push_back(i);
        }
    }
    return rows;
}

int PptPageListModel::selectedCount() const
{
    int count = 0;
    for (const auto &item : m_items)
    {
        if (item.selected)
        {
            ++count;
        }
    }
    return count;
}

void PptPageListModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }
    PptPageItem item = m_items.at(row);
    item.selected    = !item.selected;
    if (item.selected)
    {
        m_selectedPages.insert(item.pageIndex);
    }
    else
    {
        m_selectedPages.remove(item.pageIndex);
    }
    m_items.replace(row, item);
    const QModelIndex idx = index(row);
    emit              dataChanged(idx, idx, {SelectedRole});
    emit              selectedCountChanged();
    emit              selectedPagesTextChanged();
}

void PptPageListModel::setSelected(int row, bool selected)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }
    PptPageItem item = m_items.at(row);
    if (item.selected == selected)
    {
        return;
    }
    item.selected = selected;
    if (selected)
    {
        m_selectedPages.insert(item.pageIndex);
    }
    else
    {
        m_selectedPages.remove(item.pageIndex);
    }
    m_items.replace(row, item);
    const QModelIndex idx = index(row);
    emit              dataChanged(idx, idx, {SelectedRole});
    emit              selectedCountChanged();
    emit              selectedPagesTextChanged();
}

void PptPageListModel::clearSelection()
{
    const bool any = !m_selectedPages.isEmpty();
    m_selectedPages.clear();
    for (auto &m_item : m_items)
    {
        if (m_item.selected)
        {
            m_item.selected = false;
        }
    }
    if (!any)
    {
        return;
    }
    if (!m_items.isEmpty())
    {
        emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1), {SelectedRole});
    }
    emit selectedCountChanged();
    emit selectedPagesTextChanged();
}

void PptPageListModel::clear()
{
    if (m_items.isEmpty() && m_selectedPages.isEmpty())
    {
        return;
    }
    beginResetModel();
    m_items.clear();
    m_selectedPages.clear();
    endResetModel();
    emit countChanged();
    emit selectedCountChanged();
    emit selectedPagesTextChanged();
}

QString PptPageListModel::selectedPagesText() const
{
    QList<int> pages = m_selectedPages.values();
    std::sort(pages.begin(), pages.end());
    QStringList parts;
    parts.reserve(pages.size());
    for (int page : pages)
    {
        parts << QString::number(page);
    }
    return parts.join(QLatin1Char(','));
}

void PptPageListModel::setSelectedPagesText(const QString &text)
{
    QSet<int>         requested;
    const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &token : tokens)
    {
        bool      conversionSucceeded = false;
        const int page                = token.trimmed().toInt(&conversionSucceeded);
        if (conversionSucceeded && page > 0)
        {
            requested.insert(page);
        }
    }

    const bool pagesChanged = requested != m_selectedPages;
    m_selectedPages         = requested;
    bool itemChanged        = false;
    for (auto &m_item : m_items)
    {
        const bool wantSel = requested.contains(m_item.pageIndex);
        if (m_item.selected != wantSel)
        {
            m_item.selected = wantSel;
            itemChanged     = true;
        }
    }
    if (!pagesChanged && !itemChanged)
    {
        return;
    }
    if (itemChanged)
    {
        emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1), {SelectedRole});
        emit selectedCountChanged();
    }
    emit selectedPagesTextChanged();
}
