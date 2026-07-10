#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class ImageMetadata : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString imagePath READ imagePath WRITE setImagePath NOTIFY imagePathChanged)
    Q_PROPERTY(QVariantList fileInfo READ fileInfo NOTIFY metadataChanged)
    Q_PROPERTY(QVariantList imageInfo READ imageInfo NOTIFY metadataChanged)
    Q_PROPERTY(QVariantList iptc READ iptc NOTIFY metadataChanged)
    Q_PROPERTY(QVariantList exif READ exif NOTIFY metadataChanged)

public:
    explicit ImageMetadata(QObject *parent = nullptr);

    QString imagePath() const
    {
        return m_imagePath;
    }
    QVariantList fileInfo() const
    {
        return m_fileInfo;
    }
    QVariantList imageInfo() const
    {
        return m_imageInfo;
    }
    QVariantList iptc() const
    {
        return m_iptc;
    }
    QVariantList exif() const
    {
        return m_exif;
    }

    void setImagePath(const QString &path);

signals:
    void imagePathChanged();
    void metadataChanged();

private:
    void reload();

    QString      m_imagePath;
    QVariantList m_fileInfo;
    QVariantList m_imageInfo;
    QVariantList m_iptc;
    QVariantList m_exif;
};
