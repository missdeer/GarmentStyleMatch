#include "PptPageListModel.h"

#include <QSet>
#include <QStringList>

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
    emit selectedPagesTextChanged();
}

void PptPageListModel::appendItem(const PptPageItem &item)
{
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.push_back(item);
    endInsertRows();
    emit countChanged();
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
    emit selectedPagesTextChanged();
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
    emit selectedPagesTextChanged();
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
    emit selectedPagesTextChanged();
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
    emit selectedPagesTextChanged();
}

QString PptPageListModel::selectedPagesText() const
{
    QStringList parts;
    parts.reserve(m_items.size());
    for (const auto &it : m_items)
        if (it.selected)
            parts << QString::number(it.pageIndex);
    return parts.join(QLatin1Char(','));
}

void PptPageListModel::setSelectedPagesText(const QString &text)
{
    QSet<int> requested;
    const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &t : tokens) {
        bool ok = false;
        const int page = t.trimmed().toInt(&ok);
        if (ok && page > 0)
            requested.insert(page);
    }

    bool changed = false;
    for (int i = 0; i < m_items.size(); ++i) {
        const bool wantSel = requested.contains(m_items[i].pageIndex);
        if (m_items[i].selected != wantSel) {
            m_items[i].selected = wantSel;
            changed = true;
        }
    }
    if (!changed)
        return;
    emit dataChanged(index(0), index(m_items.size() - 1), {SelectedRole});
    emit selectedCountChanged();
    emit selectedPagesTextChanged();
}
