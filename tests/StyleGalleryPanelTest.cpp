#include <cstdint>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QString>
#include <QVariant>
#include <QtQuickTest>

class TestGalleryModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles : std::uint16_t
    {
        StyleIdRole = Qt::UserRole + 1,
        ImagePathRole,
        TagRole,
        IndexLabelRole,
        PartRole,
    };

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 2;
    }

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        {
            return {};
        }

        switch (role)
        {
        case StyleIdRole:
            return QStringLiteral("STYLE-%1").arg(index.row());
        case ImagePathRole:
            return QString();
        case TagRole:
            return QStringLiteral("adult");
        case IndexLabelRole:
            return index.row() + 1;
        case PartRole:
            return index.row() == 1 ? m_part : QStringLiteral("unknown");
        default:
            return {};
        }
    }

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override
    {
        return {
            {StyleIdRole, "styleId"},
            {ImagePathRole, "imagePath"},
            {TagRole, "tag"},
            {IndexLabelRole, "indexLabel"},
            {PartRole, "part"},
        };
    }

    Q_INVOKABLE void setPart(const QString &part)
    {
        beginResetModel();
        m_part = part;
        endResetModel();
    }

private:
    QString m_part = QStringLiteral("unknown");
};

class StyleGalleryPanelTestSetup final : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(QStringLiteral("galleryTestModel"), &m_model);
    }

private:
    TestGalleryModel m_model;
};

QUICK_TEST_MAIN_WITH_SETUP(stylegallerypanel, StyleGalleryPanelTestSetup)

#include "StyleGalleryPanelTest.moc"
