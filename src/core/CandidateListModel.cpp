#include "CandidateListModel.h"

#include <utility>

CandidateListModel::CandidateListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CandidateListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visibleRows.size();
}

QVariant CandidateListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size())
        return {};

    const CandidateItem &it = m_items.at(m_visibleRows.at(index.row()));
    switch (role) {
    case StyleIdRole:        return it.styleId;
    case ImagePathRole:      return it.imagePath;
    case CandidateCountRole: return it.candidateCount;
    case ScoreRole:          return it.score;
    case ConfirmedRole:      return it.confirmed;
    case DisplayLineRole:
        return QStringLiteral("%1(%2)")
            .arg(it.styleId)
            .arg(it.candidateCount);
    default: return {};
    }
}

QHash<int, QByteArray> CandidateListModel::roleNames() const
{
    return {
        {StyleIdRole,        "styleId"},
        {ImagePathRole,      "imagePath"},
        {CandidateCountRole, "candidateCount"},
        {ScoreRole,          "score"},
        {ConfirmedRole,      "confirmed"},
        {DisplayLineRole,    "displayLine"},
    };
}

void CandidateListModel::setItems(QVector<CandidateItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    rebuildVisibleRows();
    endResetModel();
    emit countChanged();
}

void CandidateListModel::setFilterText(const QString &text)
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

void CandidateListModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_filterText.isEmpty()
            || m_items.at(row).styleId.contains(m_filterText, Qt::CaseInsensitive))
            m_visibleRows.push_back(row);
    }
}

const CandidateItem *CandidateListModel::at(int row) const
{
    if (row < 0 || row >= m_visibleRows.size())
        return nullptr;
    return &m_items.at(m_visibleRows.at(row));
}

void CandidateListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    m_visibleRows.clear();
    endResetModel();
    emit countChanged();
}

void CandidateListModel::markConfirmed(int row, bool confirmed)
{
    if (row < 0 || row >= m_visibleRows.size())
        return;
    m_items[m_visibleRows.at(row)].confirmed = confirmed;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ConfirmedRole, DisplayLineRole});
}
