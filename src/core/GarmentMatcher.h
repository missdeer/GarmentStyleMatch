#pragma once

#include <atomic>
#include <functional>

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
        bool    categoryFilterEnabled = false;
    };

    struct CandidateSelection
    {
        QVector<int> indexes;
        QString      queryPart;
        QString      fallbackReason;
        int          candidatesBefore  = 0;
        int          candidatesAfter   = 0;
        int          unknownCandidates = 0;
        bool         filterEnabled     = false;
    };

    struct Match
    {
        QString styleId;
        QString imagePath;
        float   score = 0.0F;
    };

    struct Result
    {
        bool        success = false;
        QString     error;
        QString     provider;
        Match       upper;
        Match       lower;
        QStringList candidateDiagnostics;

        [[nodiscard]] QString joinedStyleIds() const;
    };

    using ResultCallback = std::function<void(int, const Result &)>;

    [[nodiscard]] static QStringList        availableProviders();
    [[nodiscard]] static QString            activeProvider();
    [[nodiscard]] static CandidateSelection selectCandidates(const QString              &queryPart,
                                                             const QVector<GalleryItem> &galleryItems,
                                                             bool                        categoryFilterEnabled);
    static void                 applyCategoryMatch(Result &result, const Match &match, const QString &queryPart, const QString &candidatePart);
    [[nodiscard]] static Result match(const QString &photoPath, const QVector<GalleryItem> &galleryItems, const Options &options);
    [[nodiscard]] static QVector<Result> matchAll(const QStringList          &photoPaths,
                                                  const QVector<GalleryItem> &galleryItems,
                                                  const Options              &options,
                                                  const std::atomic_bool     *cancellationRequested = nullptr,
                                                  int                         parallelThreadCount   = 1,
                                                  const ResultCallback       &resultCallback        = {});
};
