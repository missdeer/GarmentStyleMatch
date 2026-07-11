#pragma once

#include <cstdint>

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVector>

struct CandidateItem
{
    QString     styleId;              // e.g. "slide43_T0JE26B38A090B"
    QString     imagePath;            // absolute path to model photo
    QStringList imagePaths;           // all images in the output category directory
    int         candidateCount = 0;   // e.g. "1 张" / "2 张"
    double      score          = 0.0; // e.g. 0.9776
    bool        confirmed      = false;
};

class CandidateListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles : std::uint16_t
    {
        StyleIdRole = Qt::UserRole + 1,
        ImagePathRole,
        CandidateCountRole,
        ScoreRole,
        ConfirmedRole,
        DisplayLineRole,
    };

    explicit CandidateListModel(QObject *parent = nullptr);

    [[nodiscard]] int                    rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant               data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void                  setItems(QVector<CandidateItem> items);
    void                  setFilterText(const QString &text);
    [[nodiscard]] QString filterText() const
    {
        return m_filterText;
    }
    [[nodiscard]] const CandidateItem *at(int row) const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void markConfirmed(int row, bool confirmed = true);

signals:
    void countChanged();

private:
    void rebuildVisibleRows();

    QVector<CandidateItem> m_items;
    QVector<int>           m_visibleRows;
    QString                m_filterText;
};
