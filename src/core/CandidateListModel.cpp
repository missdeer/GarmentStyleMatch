#include <utility>

#include "CandidateListModel.h"

CandidateListModel::CandidateListModel(QObject *parent) : QAbstractListModel(parent) {}

int CandidateListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_visibleRows.size());
}

QVariant CandidateListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size())
    {
        return {};
    }

    const CandidateItem &item = m_items.at(m_visibleRows.at(index.row()));
    switch (role)
    {
    case StyleIdRole:
        return item.styleId;
    case ImagePathRole:
        return item.imagePath;
    case CandidateCountRole:
        return item.candidateCount;
    case ScoreRole:
        return item.score;
    case ConfirmedRole:
        return item.confirmed;
    case DisplayLineRole:
        return QStringLiteral("%1(%2)").arg(item.styleId).arg(item.candidateCount);
    default:
        return {};
    }
}

QHash<int, QByteArray> CandidateListModel::roleNames() const
{
    return {
        {StyleIdRole, "styleId"},
        {ImagePathRole, "imagePath"},
        {CandidateCountRole, "candidateCount"},
        {ScoreRole, "score"},
        {ConfirmedRole, "confirmed"},
        {DisplayLineRole, "displayLine"},
    };
}

void CandidateListModel::setItems(QVector<CandidateItem> items)
{
    beginResetModel();
    m_items.swap(items);
    rebuildVisibleRows();
    endResetModel();
    emit countChanged();
}

void CandidateListModel::setFilterText(const QString &text)
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

void CandidateListModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    for (int row = 0; row < m_items.size(); ++row)
    {
        if (m_filterText.isEmpty() || m_items.at(row).styleId.contains(m_filterText, Qt::CaseInsensitive))
        {
            m_visibleRows.push_back(row);
        }
    }
}

const CandidateItem *CandidateListModel::at(int row) const
{
    if (row < 0 || row >= m_visibleRows.size())
    {
        return nullptr;
    }
    return &m_items.at(m_visibleRows.at(row));
}

void CandidateListModel::clear()
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

void CandidateListModel::markConfirmed(int row, bool confirmed)
{
    if (row < 0 || row >= m_visibleRows.size())
    {
        return;
    }
    const int     itemIndex = m_visibleRows.at(row);
    CandidateItem item      = m_items.at(itemIndex);
    item.confirmed          = confirmed;
    m_items.replace(itemIndex, std::move(item));
    const QModelIndex idx = index(row);
    emit              dataChanged(idx, idx, {ConfirmedRole, DisplayLineRole});
}
