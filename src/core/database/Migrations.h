#pragma once

#include "common/Result.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace shophub {

struct Migration
{
    int version = 0;
    QString name;
    QStringList statements;
};

// Applies versioned schema migrations. Each migration runs in its own
// transaction and is recorded in schema_migrations, so upgrades are
// repeatable and never half-applied.
class Migrator
{
public:
    explicit Migrator(QString connectionName);

    Status migrate();
    int currentVersion() const;

    static const QVector<Migration> &migrations();
    static int latestVersion();

private:
    QString m_connection;
};

} // namespace shophub
