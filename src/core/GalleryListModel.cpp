#include <QDir>
#include <QFileInfo>

#include "GalleryListModel.h"

GalleryListModel::GalleryListModel(QObject *parent) : QAbstractListModel(parent) {}

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
    switch (role)
    {
    case StyleIdRole:
        return it.styleId;
    case ImagePathRole:
        return it.imagePath;
    case TagRole:
        return it.tag;
    case IndexLabelRole:
        return index.row() + 1;
    case SelectedRole:
        return index.row() == m_selectedIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> GalleryListModel::roleNames() const
{
    return {
        {StyleIdRole, "styleId"},
        {ImagePathRole, "imagePath"},
        {TagRole, "tag"},
        {IndexLabelRole, "indexLabel"},
        {SelectedRole, "selected"},
    };
}

void GalleryListModel::setItems(QVector<GalleryItem> items)
{
    const bool hadSelection = m_selectedIndex >= 0;
    beginResetModel();
    m_items         = std::move(items);
    m_selectedIndex = -1;
    endResetModel();
    emit countChanged();
    if (hadSelection)
        emit selectedIndexChanged();
}

void GalleryListModel::loadFromStyleCacheDir(const QString &directoryPath)
{
    QVector<GalleryItem> items;
    if (directoryPath.isEmpty() || !QDir(directoryPath).exists())
    {
        setItems(std::move(items));
        return;
    }

    const QDir root(directoryPath);
    const auto styleDirectories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &styleDirectoryInfo : styleDirectories)
    {
        const QString styleId = styleDirectoryInfo.fileName();
        const QDir    styleDirectory(styleDirectoryInfo.absoluteFilePath());
        const auto    imageFiles = styleDirectory.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &imageFile : imageFiles)
        {
            GalleryItem item;
            item.styleId   = styleId;
            item.imagePath = imageFile.absoluteFilePath();
            item.tag       = QStringLiteral("baby");
            items.push_back(std::move(item));
        }
    }
    setItems(std::move(items));
}

const GalleryItem *GalleryListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return &m_items.at(row);
}

void GalleryListModel::toggleSelected(int row)
{
    if (row < 0 || row >= m_items.size())
        return;

    const int previousIndex = m_selectedIndex;
    m_selectedIndex         = previousIndex == row ? -1 : row;

    if (previousIndex >= 0)
    {
        const QModelIndex previous = index(previousIndex);
        emit              dataChanged(previous, previous, {SelectedRole});
    }
    if (m_selectedIndex >= 0)
    {
        const QModelIndex current = index(m_selectedIndex);
        emit              dataChanged(current, current, {SelectedRole});
    }
    emit selectedIndexChanged();
}

void GalleryListModel::clearSelection()
{
    if (m_selectedIndex < 0)
        return;

    const QModelIndex previous = index(m_selectedIndex);
    m_selectedIndex            = -1;
    emit dataChanged(previous, previous, {SelectedRole});
    emit selectedIndexChanged();
}

void GalleryListModel::clear()
{
    if (m_items.isEmpty())
        return;
    const bool hadSelection = m_selectedIndex >= 0;
    beginResetModel();
    m_items.clear();
    m_selectedIndex = -1;
    endResetModel();
    emit countChanged();
    if (hadSelection)
        emit selectedIndexChanged();
}
