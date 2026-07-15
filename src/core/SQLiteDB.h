#pragma once

#include <QString>

struct sqlite3;
class SQLiteStatement;

class SQLiteDB final
{
public:
    enum class OpenMode
    {
        ReadOnly,
        ReadWriteCreate
    };

    explicit SQLiteDB(const QString &path, OpenMode mode = OpenMode::ReadWriteCreate);
    ~SQLiteDB();

    SQLiteDB(const SQLiteDB &)            = delete;
    SQLiteDB &operator=(const SQLiteDB &) = delete;
    SQLiteDB(SQLiteDB &&other) noexcept;
    SQLiteDB &operator=(SQLiteDB &&other) noexcept;

    [[nodiscard]] explicit        operator bool() const;
    [[nodiscard]] QString         errorMessage() const;
    [[nodiscard]] SQLiteStatement prepare(const char *sql);
    [[nodiscard]] bool            execute(const char *sql);

private:
    friend class SQLiteStatement;

    sqlite3 *m_database = nullptr;
    QString  m_openError;
};
