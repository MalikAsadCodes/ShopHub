#pragma once

#include "common/Result.h"

#include <QSqlDatabase>
#include <QString>

namespace shophub {

// Owns one named SQLite connection. Foreign keys are switched on and verified
// every time a connection is opened.
class Database
{
public:
    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // filePath may be ":memory:" for tests.
    Status open(const QString &filePath);
    void close();

    bool isOpen() const;
    const QString &connectionName() const { return m_connection; }
    QSqlDatabase handle() const;

private:
    QString m_connection;
};

} // namespace shophub
