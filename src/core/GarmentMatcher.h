#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "GalleryListModel.h"

class GarmentMatcher
{
public:
    struct Options
    {
        QString segmentationModelPath;
        QString embeddingModelPath;
        QString featureDatabasePath;
    };

    struct Match
    {
        QString styleId;
        float   score = 0.0F;
    };

    struct Result
    {
        bool    success = false;
        QString error;
        QString provider;
        Match   upper;
        Match   lower;

        [[nodiscard]] QString joinedStyleIds() const;
    };

    [[nodiscard]] static QStringList availableProviders();
    [[nodiscard]] static QString     activeProvider();
    [[nodiscard]] static Result      match(const QString &photoPath, const QVector<GalleryItem> &galleryItems, const Options &options);
};
