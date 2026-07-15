#include <array>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "MatchResultStore.h"
#include "SQLiteDB.h"
#include "SQLiteStatement.h"

// SQLite's C API exposes text as unsigned bytes, and the positional values below are fixed by the local table schema and SQL statements.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-type-reinterpret-cast,readability-magic-numbers)
namespace
{
    struct ImageIdentity
    {
        QString fileName;
        qint64  fileSize = 0;
        QString md5;
    };

    void setError(QString *error, const QString &message)
    {
        if (error)
        {
            *error = message;
        }
    }

    [[nodiscard]] std::optional<ImageIdentity> imageIdentity(const QString &imagePath, QString *error)
    {
        const QFileInfo info(imagePath);
        QFile           file(imagePath);
        if (!info.exists() || !info.isFile() || !file.open(QIODevice::ReadOnly))
        {
            setError(error, QStringLiteral("无法读取图片: %1").arg(imagePath));
            return std::nullopt;
        }

        QCryptographicHash          hash(QCryptographicHash::Md5);
        std::array<char, 64 * 1024> buffer {};
        while (!file.atEnd())
        {
            const qint64 bytesRead = file.read(buffer.data(), static_cast<qint64>(buffer.size()));
            if (bytesRead < 0)
            {
                setError(error, QStringLiteral("计算图片 MD5 失败: %1").arg(imagePath));
                return std::nullopt;
            }
            hash.addData(QByteArrayView(buffer.data(), bytesRead));
        }
        return ImageIdentity {info.fileName(), info.size(), QString::fromLatin1(hash.result().toHex())};
    }

    [[nodiscard]] bool bindIdentity(SQLiteStatement &statement, const ImageIdentity &identity)
    {
        return statement.bindText(1, identity.fileName) && statement.bindInt64(2, identity.fileSize) && statement.bindText(3, identity.md5);
    }
} // namespace

std::optional<StoredMatchResult> MatchResultStore::load(const QString &databasePath, const QString &imagePath, QString *error)
{
    if (error)
    {
        error->clear();
    }
    if (!QFileInfo::exists(databasePath))
    {
        return std::nullopt;
    }

    const auto identity = imageIdentity(imagePath, error);
    if (!identity)
    {
        return std::nullopt;
    }

    SQLiteDB database(databasePath, SQLiteDB::OpenMode::ReadOnly);
    if (!database)
    {
        setError(error, database.errorMessage());
        return std::nullopt;
    }
    SQLiteStatement statement = database.prepare("SELECT upper_style_id,upper_image_name,upper_confirmed,"
                                                 "lower_style_id,lower_image_name,lower_confirmed FROM garment_matches "
                                                 "WHERE photo_file_name=?1 AND photo_file_size=?2 AND photo_md5=?3");
    if (!statement)
    {
        setError(error, database.errorMessage());
        return std::nullopt;
    }
    if (!bindIdentity(statement, *identity))
    {
        setError(error, statement.errorMessage());
        return std::nullopt;
    }

    const SQLiteStatement::StepResult stepResult = statement.step();
    if (stepResult == SQLiteStatement::StepResult::Done)
    {
        return std::nullopt;
    }
    if (stepResult != SQLiteStatement::StepResult::Row)
    {
        setError(error, statement.errorMessage());
        return std::nullopt;
    }

    StoredMatchResult result;
    result.photoFileName   = identity->fileName;
    result.photoFileSize   = identity->fileSize;
    result.photoMd5        = identity->md5;
    result.upper.styleId   = statement.columnText(0);
    result.upper.imageName = statement.columnText(1);
    result.upper.confirmed = statement.columnInt(2) != 0;
    result.lower.styleId   = statement.columnText(3);
    result.lower.imageName = statement.columnText(4);
    result.lower.confirmed = statement.columnInt(5) != 0;
    return result;
}

bool MatchResultStore::save(const QString &databasePath, const QString &imagePath, const StoredMatchResult &result, QString *error)
{
    if (error)
    {
        error->clear();
    }
    const auto identity = imageIdentity(imagePath, error);
    if (!identity)
    {
        return false;
    }

    QDir().mkpath(QFileInfo(databasePath).absolutePath());
    SQLiteDB database(databasePath);
    if (!database)
    {
        setError(error, database.errorMessage());
        return false;
    }
    if (!database.execute("CREATE TABLE IF NOT EXISTS garment_matches("
                          "photo_file_name TEXT NOT NULL,photo_file_size INTEGER NOT NULL,photo_md5 TEXT NOT NULL,"
                          "upper_style_id TEXT NOT NULL,upper_image_name TEXT NOT NULL,upper_confirmed INTEGER NOT NULL,"
                          "lower_style_id TEXT NOT NULL,lower_image_name TEXT NOT NULL,lower_confirmed INTEGER NOT NULL,"
                          "PRIMARY KEY(photo_file_name,photo_file_size,photo_md5))"))
    {
        setError(error, database.errorMessage());
        return false;
    }

    if (result.isEmpty())
    {
        SQLiteStatement statement = database.prepare("DELETE FROM garment_matches WHERE photo_file_name=?1 AND photo_file_size=?2 AND photo_md5=?3");
        if (!statement)
        {
            setError(error, database.errorMessage());
            return false;
        }
        if (!bindIdentity(statement, *identity))
        {
            setError(error, statement.errorMessage());
            return false;
        }
        if (statement.step() == SQLiteStatement::StepResult::Done)
        {
            return true;
        }
        setError(error, statement.errorMessage());
        return false;
    }

    SQLiteStatement statement = database.prepare("INSERT INTO garment_matches(photo_file_name,photo_file_size,photo_md5,"
                                                 "upper_style_id,upper_image_name,upper_confirmed,lower_style_id,lower_image_name,lower_confirmed) "
                                                 "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9) ON CONFLICT(photo_file_name,photo_file_size,photo_md5) "
                                                 "DO UPDATE SET upper_style_id=excluded.upper_style_id,upper_image_name=excluded.upper_image_name,"
                                                 "upper_confirmed=excluded.upper_confirmed,lower_style_id=excluded.lower_style_id,"
                                                 "lower_image_name=excluded.lower_image_name,lower_confirmed=excluded.lower_confirmed");
    if (!statement)
    {
        setError(error, database.errorMessage());
        return false;
    }
    if (!bindIdentity(statement, *identity) || !statement.bindText(4, result.upper.styleId) || !statement.bindText(5, result.upper.imageName) ||
        !statement.bindInt(6, result.upper.confirmed ? 1 : 0) || !statement.bindText(7, result.lower.styleId) ||
        !statement.bindText(8, result.lower.imageName) || !statement.bindInt(9, result.lower.confirmed ? 1 : 0))
    {
        setError(error, statement.errorMessage());
        return false;
    }
    if (statement.step() == SQLiteStatement::StepResult::Done)
    {
        return true;
    }
    setError(error, statement.errorMessage());
    return false;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-type-reinterpret-cast,readability-magic-numbers)
