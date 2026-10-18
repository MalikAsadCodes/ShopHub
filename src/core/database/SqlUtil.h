#pragma once

#include "common/Result.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace shophub::sql {

// Shown to users when something fails inside the database. The real error is logged.
inline const QString kGenericDbError = QStringLiteral("A database error occurred. Please try again.");

// Executes an already prepared query. On failure the technical error is logged
// with the given context and a user-safe Status is returned.
Status exec(QSqlQuery &query, const char *context);

// Logs the last error of a query that already failed and returns a user-safe Status.
Status failure(const QSqlQuery &query, const char *context);

// Prepares and executes a statement without bound values.
Status execRaw(const QSqlDatabase &db, const QString &statement, const char *context);

bool isUniqueViolation(const QSqlQuery &query);

QString nowUtc();                                  // ISO-8601, UTC, e.g. 2026-10-17T15:30:00Z
QString toDb(const QDateTime &dt);                 // ISO-8601 UTC or empty
QDateTime fromDb(const QVariant &value);           // invalid for NULL/empty

QVariant nullable(qint64 value);                   // NULL when value <= 0

// Binds text as '' instead of NULL. (Qt binds a default-constructed QString as NULL.)
QVariant text(const QString &value);

} // namespace shophub::sql
