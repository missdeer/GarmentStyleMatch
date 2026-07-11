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
    return m_items.size();
}

QVariant CandidateListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const CandidateItem &it = m_items.at(index.row());
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
    endResetModel();
    emit countChanged();
}

const CandidateItem *CandidateListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

void CandidateListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

void CandidateListModel::markConfirmed(int row, bool confirmed)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_items[row].confirmed = confirmed;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ConfirmedRole, DisplayLineRole});
}
