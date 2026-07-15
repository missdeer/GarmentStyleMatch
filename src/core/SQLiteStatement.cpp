#include <utility>
#include <sqlite3.h>

#include "SQLiteStatement.h"

SQLiteStatement::SQLiteStatement(sqlite3 *database, const char *sql) : m_database(database)
{
    if (m_database)
    {
        sqlite3_prepare_v2(m_database, sql, -1, &m_statement, nullptr);
    }
}

SQLiteStatement::~SQLiteStatement()
{
    if (m_statement)
    {
        sqlite3_finalize(m_statement);
    }
}

SQLiteStatement::SQLiteStatement(SQLiteStatement &&other) noexcept
    : m_database(std::exchange(other.m_database, nullptr)), m_statement(std::exchange(other.m_statement, nullptr))
{
}

SQLiteStatement &SQLiteStatement::operator=(SQLiteStatement &&other) noexcept
{
    if (this != &other)
    {
        if (m_statement)
        {
            sqlite3_finalize(m_statement);
        }
        m_database  = std::exchange(other.m_database, nullptr);
        m_statement = std::exchange(other.m_statement, nullptr);
    }
    return *this;
}

SQLiteStatement::operator bool() const
{
    return m_statement != nullptr;
}

QString SQLiteStatement::errorMessage() const
{
    return m_database ? QString::fromUtf8(sqlite3_errmsg(m_database)) : QStringLiteral("SQLite 数据库未打开");
}

bool SQLiteStatement::bindText(int index, const QString &value)
{
    const QByteArray encoded = value.toUtf8();
    return m_statement && sqlite3_bind_text(m_statement, index, encoded.constData(), static_cast<int>(encoded.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool SQLiteStatement::bindInt(int index, int value)
{
    return m_statement && sqlite3_bind_int(m_statement, index, value) == SQLITE_OK;
}

bool SQLiteStatement::bindInt64(int index, qint64 value)
{
    return m_statement && sqlite3_bind_int64(m_statement, index, value) == SQLITE_OK;
}

bool SQLiteStatement::bindBlob(int index, const void *data, int bytes)
{
    return m_statement && sqlite3_bind_blob(m_statement, index, data, bytes, SQLITE_TRANSIENT) == SQLITE_OK;
}

SQLiteStatement::StepResult SQLiteStatement::step()
{
    const int result = m_statement ? sqlite3_step(m_statement) : SQLITE_MISUSE;
    if (result == SQLITE_ROW)
    {
        return StepResult::Row;
    }
    if (result == SQLITE_DONE)
    {
        return StepResult::Done;
    }
    return StepResult::Error;
}

QString SQLiteStatement::columnText(int column) const
{
    const auto *value = m_statement ? sqlite3_column_text(m_statement, column) : nullptr;
    return value ? QString::fromUtf8(reinterpret_cast<const char *>(value)) : QString();
}

int SQLiteStatement::columnInt(int column) const
{
    return m_statement ? sqlite3_column_int(m_statement, column) : 0;
}

int SQLiteStatement::columnBytes(int column) const
{
    return m_statement ? sqlite3_column_bytes(m_statement, column) : 0;
}

const void *SQLiteStatement::columnBlob(int column) const
{
    return m_statement ? sqlite3_column_blob(m_statement, column) : nullptr;
}
