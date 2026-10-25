#include "database/SqlUtil.h"

#include <QSqlError>
#include <QtDebug>

namespace shophub::sql {

Status exec(QSqlQuery &query, const char *context)
{
    if (query.exec())
        return Status::ok();
    return failure(query, context);
}

Status failure(const QSqlQuery &query, const char *context)
{
    qWarning().noquote() << "SQL error in" << context << ":" << query.lastError().text();
    if (isUniqueViolation(query))
        return Status::fail(QStringLiteral("This record already exists."));
    return Status::fail(kGenericDbError);
}

Status execRaw(const QSqlDatabase &db, const QString &statement, const char *context)
{
    QSqlQuery q(db);
    if (!q.exec(statement)) {
        qWarning().noquote() << "SQL error in" << context << ":" << q.lastError().text();
        return Status::fail(kGenericDbError);
    }
    return Status::ok();
}

bool isUniqueViolation(const QSqlQuery &query)
{
    return query.lastError().databaseText().contains(QLatin1String("UNIQUE constraint failed"));
}

namespace {
QString g_fixedNow; // empty = use the real clock
}

QString nowUtc()
{
    return g_fixedNow.isEmpty() ? toDb(QDateTime::currentDateTimeUtc()) : g_fixedNow;
}

QDate todayLocal()
{
    // Follows ScopedFixedClock too, so backdated sample qists get realistic due dates.
    return fromDb(nowUtc()).toLocalTime().date();
}

ScopedFixedClock::ScopedFixedClock(const QDateTime &utc)
    : m_previous(g_fixedNow)
{
    g_fixedNow = toDb(utc);
}

ScopedFixedClock::~ScopedFixedClock()
{
    g_fixedNow = m_previous;
}

QString toDb(const QDateTime &dt)
{
    if (!dt.isValid())
        return QString();
    return dt.toUTC().toString(Qt::ISODate);
}

QDateTime fromDb(const QVariant &value)
{
    if (value.isNull())
        return QDateTime();
    const QString s = value.toString();
    if (s.isEmpty())
        return QDateTime();
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (dt.isValid() && dt.timeSpec() != Qt::UTC)
        dt.setTimeSpec(Qt::UTC);
    return dt;
}

QVariant nullable(qint64 value)
{
    return value > 0 ? QVariant(value) : QVariant(QMetaType(QMetaType::LongLong));
}

QVariant text(const QString &value)
{
    // QStringLiteral("") is empty but not null, so it binds as ''.
    return QVariant(value.isNull() ? QStringLiteral("") : value);
}

} // namespace shophub::sql
