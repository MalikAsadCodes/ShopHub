#include "database/Transaction.h"

#include "database/SqlUtil.h"

#include <QSqlDatabase>
#include <QtDebug>

namespace shophub {

Transaction::Transaction(QString connectionName)
    : m_connection(std::move(connectionName))
{
}

Transaction::~Transaction()
{
    if (m_active)
        rollback();
}

Status Transaction::begin()
{
    if (m_active)
        return Status::ok();
    const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    // IMMEDIATE takes the write lock up-front, so two writers can never
    // both read the same stock level and then both decrement it.
    if (auto st = sql::execRaw(db, QStringLiteral("BEGIN IMMEDIATE"), "Transaction::begin"); !st)
        return st;
    m_active = true;
    return Status::ok();
}

Status Transaction::commit()
{
    if (!m_active)
        return Status::fail(QStringLiteral("No active transaction."));
    const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    if (auto st = sql::execRaw(db, QStringLiteral("COMMIT"), "Transaction::commit"); !st) {
        rollback();
        return st;
    }
    m_active = false;
    return Status::ok();
}

void Transaction::rollback()
{
    if (!m_active)
        return;
    const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    sql::execRaw(db, QStringLiteral("ROLLBACK"), "Transaction::rollback");
    m_active = false;
}

} // namespace shophub
