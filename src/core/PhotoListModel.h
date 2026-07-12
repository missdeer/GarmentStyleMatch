#pragma once

#include <cstdint>

#include <QAbstractListModel>
#include <QString>
#include <QVector>

enum class PhotoMatchStatus : std::uint8_t
{
    Unmatched,
    Matched,
    Confirmed,
};

struct PhotoItem
{
    QString          fileName;          // 文件名(不含目录)
    QString          imagePath;         // 绝对路径
    bool             processed = false; // 是否已归类
    QString          relativePath;      // 相对于实拍图片目录的路径
    PhotoMatchStatus upperMatchStatus = PhotoMatchStatus::Unmatched;
    PhotoMatchStatus lowerMatchStatus = PhotoMatchStatus::Unmatched;
};

class PhotoListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles : std::uint16_t
    {
        FileNameRole = Qt::UserRole + 1,
        ImagePathRole,
        ProcessedRole,
        DisplayLineRole,
        UpperMatchStatusRole,
        LowerMatchStatusRole,
    };

    explicit PhotoListModel(QObject *parent = nullptr);

    [[nodiscard]] int                    rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant               data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void                  setItems(QVector<PhotoItem> items);
    void                  setMatchStatuses(const QString &imagePath, PhotoMatchStatus upper, PhotoMatchStatus lower);
    void                  setFilterText(const QString &text);
    [[nodiscard]] QString filterText() const
    {
        return m_filterText;
    }
    [[nodiscard]] const PhotoItem          *at(int row) const;
    [[nodiscard]] const QVector<PhotoItem> &allItems() const
    {
        return m_items;
    }

    Q_INVOKABLE void clear();
    Q_INVOKABLE void markProcessed(int row, bool processed = true);

signals:
    void countChanged();

private:
    void rebuildVisibleRows();

    QVector<PhotoItem> m_items;
    QVector<int>       m_visibleRows;
    QString            m_filterText;
};
