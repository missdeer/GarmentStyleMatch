#include <utility>

#include <QDir>
#include <QFileInfo>

#include "GalleryListModel.h"

GalleryListModel::GalleryListModel(QObject *parent) : QAbstractListModel(parent) {}

int GalleryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant GalleryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    {
        return {};
    }

    const GalleryItem &item = m_items.at(index.row());
    switch (role)
    {
    case StyleIdRole:
        return item.styleId;
    case ImagePathRole:
        return item.imagePath;
    case TagRole:
        return item.tag;
    case IndexLabelRole:
        return index.row() + 1;
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
    };
}

void GalleryListModel::setItems(QVector<GalleryItem> items)
{
    beginResetModel();
    m_allItems.swap(items);
    rebuildFilteredItems();
    endResetModel();
    emit countChanged();
}

void GalleryListModel::setFilterText(const QString &text)
{
    const QString normalizedText = text.trimmed();
    if (normalizedText == m_filterText)
    {
        return;
    }

    beginResetModel();
    m_filterText = normalizedText;
    rebuildFilteredItems();
    endResetModel();
    emit countChanged();
}

void GalleryListModel::rebuildFilteredItems()
{
    m_items.clear();
    if (m_filterText.isEmpty())
    {
        m_items = m_allItems;
        return;
    }

    for (const GalleryItem &item : std::as_const(m_allItems))
    {
        if (item.styleId.contains(m_filterText, Qt::CaseInsensitive))
        {
            m_items.push_back(item);
        }
    }
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
    {
        return nullptr;
    }
    return &m_items.at(row);
}

void GalleryListModel::clear()
{
    if (m_allItems.isEmpty())
    {
        return;
    }
    beginResetModel();
    m_allItems.clear();
    m_items.clear();
    endResetModel();
    emit countChanged();
}
