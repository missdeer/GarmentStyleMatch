#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QString>

struct PhotoItem
{
    QString fileName;   // 文件名(不含目录)
    QString imagePath;  // 绝对路径
    bool    processed = false; // 是否已归类
};

class PhotoListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        FileNameRole = Qt::UserRole + 1,
        ImagePathRole,
        ProcessedRole,
        DisplayLineRole,
    };

    explicit PhotoListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<PhotoItem> items);
    const PhotoItem *at(int row) const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void markProcessed(int row, bool processed = true);

signals:
    void countChanged();

private:
    QVector<PhotoItem> m_items;
};
