#pragma once

#include <QString>

struct sqlite3;
struct sqlite3_stmt;

class SQLiteDB;

class SQLiteStatement final
{
public:
    enum class StepResult
    {
        Row,
        Done,
        Error
    };

    SQLiteStatement() = default;
    ~SQLiteStatement();

    SQLiteStatement(const SQLiteStatement &)            = delete;
    SQLiteStatement &operator=(const SQLiteStatement &) = delete;
    SQLiteStatement(SQLiteStatement &&other) noexcept;
    SQLiteStatement &operator=(SQLiteStatement &&other) noexcept;

    [[nodiscard]] explicit    operator bool() const;
    [[nodiscard]] QString     errorMessage() const;
    [[nodiscard]] bool        bindText(int index, const QString &value);
    [[nodiscard]] bool        bindInt(int index, int value);
    [[nodiscard]] bool        bindInt64(int index, qint64 value);
    [[nodiscard]] bool        bindBlob(int index, const void *data, int bytes);
    [[nodiscard]] StepResult  step();
    [[nodiscard]] QString     columnText(int column) const;
    [[nodiscard]] int         columnInt(int column) const;
    [[nodiscard]] int         columnBytes(int column) const;
    [[nodiscard]] const void *columnBlob(int column) const;

private:
    friend class SQLiteDB;

    SQLiteStatement(sqlite3 *database, const char *sql);

    sqlite3      *m_database  = nullptr;
    sqlite3_stmt *m_statement = nullptr;
};
