#pragma once

#include <optional>

#include <QString>

struct StoredGarmentMatch
{
    QString styleId;
    QString imageName;
    bool    confirmed = false;

    [[nodiscard]] bool isEmpty() const
    {
        return styleId.isEmpty();
    }
};

struct StoredMatchResult
{
    QString            photoFileName;
    qint64             photoFileSize = 0;
    QString            photoMd5;
    StoredGarmentMatch upper;
    StoredGarmentMatch lower;

    [[nodiscard]] bool isEmpty() const
    {
        return upper.isEmpty() && lower.isEmpty();
    }

    [[nodiscard]] bool allMatchesConfirmed() const
    {
        return !upper.isEmpty() && upper.confirmed && !lower.isEmpty() && lower.confirmed;
    }

    void replaceUnconfirmedMatches(const StoredMatchResult &replacement)
    {
        if (!upper.confirmed)
            upper = replacement.upper;
        if (!lower.confirmed)
            lower = replacement.lower;
    }
};

class MatchResultStore final
{
public:
    [[nodiscard]] static std::optional<StoredMatchResult> load(const QString &databasePath, const QString &imagePath, QString *error = nullptr);
    [[nodiscard]] static bool save(const QString &databasePath, const QString &imagePath, const StoredMatchResult &result, QString *error = nullptr);
};
