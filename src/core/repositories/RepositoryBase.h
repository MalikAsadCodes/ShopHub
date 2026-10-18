#pragma once

#include <QSqlDatabase>
#include <QString>

namespace shophub {

// Repositories are the only classes that contain SQL. They look up the
// connection by name on every call instead of holding a QSqlDatabase,
// so the connection can be closed cleanly at shutdown.
class RepositoryBase
{
public:
    explicit RepositoryBase(QString connectionName) : m_connection(std::move(connectionName)) {}

protected:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connection, false); }

private:
    QString m_connection;
};

} // namespace shophub
