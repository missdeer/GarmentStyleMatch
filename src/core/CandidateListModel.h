#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVector>
#include <QString>

struct CandidateItem
{
    QString styleId;        // e.g. "slide43_T0JE26B38A090B"
    QString imagePath;      // absolute path to model photo
    QStringList imagePaths; // all images in the output category directory
    int     candidateCount = 0;   // e.g. "1 张" / "2 张"
    double  score          = 0.0; // e.g. 0.9776
    bool    confirmed      = false;
};

class CandidateListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        StyleIdRole = Qt::UserRole + 1,
        ImagePathRole,
        CandidateCountRole,
        ScoreRole,
        ConfirmedRole,
        DisplayLineRole,
    };

    explicit CandidateListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<CandidateItem> items);
    const CandidateItem *at(int row) const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void markConfirmed(int row, bool confirmed = true);

signals:
    void countChanged();

private:
    QVector<CandidateItem> m_items;
};
