#include <utility>
#include <sqlite3.h>

#include "SQLiteDB.h"
#include "SQLiteStatement.h"

SQLiteDB::SQLiteDB(const QString &path, OpenMode mode)
{
    const QByteArray encodedPath = path.toUtf8();
    const int        flags       = mode == OpenMode::ReadOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(encodedPath.constData(), &m_database, flags, nullptr) != SQLITE_OK)
    {
        m_openError = m_database ? QString::fromUtf8(sqlite3_errmsg(m_database)) : QStringLiteral("无法打开 SQLite 数据库");
        if (m_database)
        {
            sqlite3_close(m_database);
            m_database = nullptr;
        }
    }
}

SQLiteDB::~SQLiteDB()
{
    if (m_database)
    {
        sqlite3_close(m_database);
    }
}

SQLiteDB::SQLiteDB(SQLiteDB &&other) noexcept : m_database(std::exchange(other.m_database, nullptr)), m_openError(std::move(other.m_openError)) {}

SQLiteDB &SQLiteDB::operator=(SQLiteDB &&other) noexcept
{
    if (this != &other)
    {
        if (m_database)
        {
            sqlite3_close(m_database);
        }
        m_database  = std::exchange(other.m_database, nullptr);
        m_openError = std::move(other.m_openError);
    }
    return *this;
}

SQLiteDB::operator bool() const
{
    return m_database != nullptr;
}

QString SQLiteDB::errorMessage() const
{
    return m_database ? QString::fromUtf8(sqlite3_errmsg(m_database)) : m_openError;
}

SQLiteStatement SQLiteDB::prepare(const char *sql)
{
    return SQLiteStatement(m_database, sql);
}

bool SQLiteDB::execute(const char *sql)
{
    return m_database && sqlite3_exec(m_database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}
