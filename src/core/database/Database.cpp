#include "database/Database.h"

#include "database/SqlUtil.h"

#include <QAtomicInt>
#include <QSqlError>
#include <QSqlQuery>
#include <QtDebug>

namespace shophub {

namespace {
QAtomicInt g_connectionCounter;
}

Database::Database()
    : m_connection(QStringLiteral("shophub_%1").arg(g_connectionCounter.fetchAndAddRelaxed(1)))
{
}

Database::~Database()
{
    close();
}

Status Database::open(const QString &filePath)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        qCritical() << "Qt SQLite driver (QSQLITE) is not available";
        return Status::fail(QStringLiteral("The SQLite database driver is missing from this installation."));
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        qCritical().noquote() << "Cannot open database" << filePath << ":" << db.lastError().text();
        return Status::fail(QStringLiteral("Could not open the database file."));
    }

    const bool inMemory = filePath == QLatin1String(":memory:");
    if (auto st = sql::execRaw(db, QStringLiteral("PRAGMA foreign_keys = ON"), "Database::open"); !st)
        return st;
    if (auto st = sql::execRaw(db, QStringLiteral("PRAGMA busy_timeout = 5000"), "Database::open"); !st)
        return st;
    if (!inMemory) {
        if (auto st = sql::execRaw(db, QStringLiteral("PRAGMA journal_mode = WAL"), "Database::open"); !st)
            return st;
    }

    // Verify foreign keys are really enforced (they are silently ignored if
    // SQLite was compiled without support).
    QSqlQuery check(db);
    if (!check.exec(QStringLiteral("PRAGMA foreign_keys")) || !check.next() || check.value(0).toInt() != 1) {
        qCritical() << "SQLite foreign key enforcement could not be enabled";
        return Status::fail(QStringLiteral("The database could not enable data-integrity checks."));
    }

    qInfo().noquote() << "Database opened:" << (inMemory ? QStringLiteral("(in memory)") : filePath);
    return Status::ok();
}

void Database::close()
{
    if (!QSqlDatabase::contains(m_connection))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connection) && QSqlDatabase::database(m_connection, false).isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connection, false);
}

} // namespace shophub
