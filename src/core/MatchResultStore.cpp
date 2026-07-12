#include <array>
#include <memory>
#include <sqlite3.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "MatchResultStore.h"

// SQLite's C API exposes text as unsigned bytes, and the positional values below are fixed by the local table schema and SQL statements.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-type-reinterpret-cast,readability-magic-numbers)
namespace
{
    struct SqliteDeleter
    {
        void operator()(sqlite3 *database) const
        {
            if (database)
            {
                sqlite3_close(database);
            }
        }
    };

    struct StatementDeleter
    {
        void operator()(sqlite3_stmt *statement) const
        {
            if (statement)
            {
                sqlite3_finalize(statement);
            }
        }
    };

    using Database  = std::unique_ptr<sqlite3, SqliteDeleter>;
    using Statement = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

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

    [[nodiscard]] Database openDatabase(const QString &path, int flags, QString *error)
    {
        sqlite3         *rawDatabase = nullptr;
        const QByteArray encodedPath = path.toUtf8();
        if (sqlite3_open_v2(encodedPath.constData(), &rawDatabase, flags, nullptr) != SQLITE_OK)
        {
            const QString message = rawDatabase ? QString::fromUtf8(sqlite3_errmsg(rawDatabase)) : QStringLiteral("无法打开 SQLite 数据库");
            if (rawDatabase)
            {
                sqlite3_close(rawDatabase);
            }
            setError(error, message);
            return {};
        }
        return Database(rawDatabase);
    }

    [[nodiscard]] Statement prepare(sqlite3 *database, const char *sql, QString *error)
    {
        sqlite3_stmt *rawStatement = nullptr;
        if (sqlite3_prepare_v2(database, sql, -1, &rawStatement, nullptr) != SQLITE_OK)
        {
            setError(error, QString::fromUtf8(sqlite3_errmsg(database)));
            return {};
        }
        return Statement(rawStatement);
    }

    bool execute(sqlite3 *database, const char *sql, QString *error)
    {
        char *rawError = nullptr;
        if (sqlite3_exec(database, sql, nullptr, nullptr, &rawError) == SQLITE_OK)
        {
            return true;
        }

        setError(error, QString::fromUtf8(rawError ? rawError : sqlite3_errmsg(database)));
        sqlite3_free(rawError);
        return false;
    }

    void bindText(sqlite3_stmt *statement, int index, const QString &value)
    {
        const QByteArray encoded = value.toUtf8();
        sqlite3_bind_text(statement, index, encoded.constData(), static_cast<int>(encoded.size()), SQLITE_TRANSIENT);
    }

    [[nodiscard]] QString columnText(sqlite3_stmt *statement, int column)
    {
        const auto *text = sqlite3_column_text(statement, column);
        return text ? QString::fromUtf8(reinterpret_cast<const char *>(text)) : QString();
    }

    void bindIdentity(sqlite3_stmt *statement, const ImageIdentity &identity)
    {
        bindText(statement, 1, identity.fileName);
        sqlite3_bind_int64(statement, 2, identity.fileSize);
        bindText(statement, 3, identity.md5);
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

    Database database = openDatabase(databasePath, SQLITE_OPEN_READONLY, error);
    if (!database)
    {
        return std::nullopt;
    }
    Statement statement = prepare(database.get(),
                                  "SELECT upper_style_id,upper_image_name,upper_confirmed,"
                                  "lower_style_id,lower_image_name,lower_confirmed FROM garment_matches "
                                  "WHERE photo_file_name=?1 AND photo_file_size=?2 AND photo_md5=?3",
                                  error);
    if (!statement)
    {
        return std::nullopt;
    }
    bindIdentity(statement.get(), *identity);

    const int stepResult = sqlite3_step(statement.get());
    if (stepResult == SQLITE_DONE)
    {
        return std::nullopt;
    }
    if (stepResult != SQLITE_ROW)
    {
        setError(error, QString::fromUtf8(sqlite3_errmsg(database.get())));
        return std::nullopt;
    }

    StoredMatchResult result;
    result.photoFileName   = identity->fileName;
    result.photoFileSize   = identity->fileSize;
    result.photoMd5        = identity->md5;
    result.upper.styleId   = columnText(statement.get(), 0);
    result.upper.imageName = columnText(statement.get(), 1);
    result.upper.confirmed = sqlite3_column_int(statement.get(), 2) != 0;
    result.lower.styleId   = columnText(statement.get(), 3);
    result.lower.imageName = columnText(statement.get(), 4);
    result.lower.confirmed = sqlite3_column_int(statement.get(), 5) != 0;
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
    Database database = openDatabase(databasePath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, error);
    if (!database)
    {
        return false;
    }
    if (!execute(database.get(),
                 "CREATE TABLE IF NOT EXISTS garment_matches("
                 "photo_file_name TEXT NOT NULL,photo_file_size INTEGER NOT NULL,photo_md5 TEXT NOT NULL,"
                 "upper_style_id TEXT NOT NULL,upper_image_name TEXT NOT NULL,upper_confirmed INTEGER NOT NULL,"
                 "lower_style_id TEXT NOT NULL,lower_image_name TEXT NOT NULL,lower_confirmed INTEGER NOT NULL,"
                 "PRIMARY KEY(photo_file_name,photo_file_size,photo_md5))",
                 error))
    {
        return false;
    }

    if (result.isEmpty())
    {
        Statement statement =
            prepare(database.get(), "DELETE FROM garment_matches WHERE photo_file_name=?1 AND photo_file_size=?2 AND photo_md5=?3", error);
        if (!statement)
        {
            return false;
        }
        bindIdentity(statement.get(), *identity);
        if (sqlite3_step(statement.get()) == SQLITE_DONE)
        {
            return true;
        }
        setError(error, QString::fromUtf8(sqlite3_errmsg(database.get())));
        return false;
    }

    Statement statement = prepare(database.get(),
                                  "INSERT INTO garment_matches(photo_file_name,photo_file_size,photo_md5,"
                                  "upper_style_id,upper_image_name,upper_confirmed,lower_style_id,lower_image_name,lower_confirmed) "
                                  "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9) ON CONFLICT(photo_file_name,photo_file_size,photo_md5) "
                                  "DO UPDATE SET upper_style_id=excluded.upper_style_id,upper_image_name=excluded.upper_image_name,"
                                  "upper_confirmed=excluded.upper_confirmed,lower_style_id=excluded.lower_style_id,"
                                  "lower_image_name=excluded.lower_image_name,lower_confirmed=excluded.lower_confirmed",
                                  error);
    if (!statement)
    {
        return false;
    }
    bindIdentity(statement.get(), *identity);
    bindText(statement.get(), 4, result.upper.styleId);
    bindText(statement.get(), 5, result.upper.imageName);
    sqlite3_bind_int(statement.get(), 6, result.upper.confirmed ? 1 : 0);
    bindText(statement.get(), 7, result.lower.styleId);
    bindText(statement.get(), 8, result.lower.imageName);
    sqlite3_bind_int(statement.get(), 9, result.lower.confirmed ? 1 : 0);
    if (sqlite3_step(statement.get()) == SQLITE_DONE)
    {
        return true;
    }
    setError(error, QString::fromUtf8(sqlite3_errmsg(database.get())));
    return false;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-type-reinterpret-cast,readability-magic-numbers)
