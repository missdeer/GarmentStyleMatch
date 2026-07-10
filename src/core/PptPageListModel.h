#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QString>
#include <QVector>

struct PptPageItem
{
    int     pageIndex = 0;   // 1-based
    QString imagePath;       // rendered thumbnail path (empty for stub)
    bool    selected  = false;
};

class PptPageListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int     count             READ rowCount            NOTIFY countChanged)
    Q_PROPERTY(int     selectedCount     READ selectedCount       NOTIFY selectedCountChanged)
    Q_PROPERTY(QString selectedPagesText READ selectedPagesText   WRITE setSelectedPagesText NOTIFY selectedPagesTextChanged)

public:
    enum Roles {
        PageIndexRole = Qt::UserRole + 1,
        ImagePathRole,
        SelectedRole,
    };

    explicit PptPageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<PptPageItem> items);
    void appendItem(const PptPageItem &item);
    const PptPageItem *at(int row) const;
    QVector<int> selectedRows() const;
    int selectedCount() const;
    QString selectedPagesText() const;
    void    setSelectedPagesText(const QString &text);

    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void setSelected(int row, bool on);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void selectedCountChanged();
    void selectedPagesTextChanged();

private:
    QVector<PptPageItem> m_items;
    QSet<int>            m_selectedPages;
};
