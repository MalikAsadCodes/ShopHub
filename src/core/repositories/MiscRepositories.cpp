#include "repositories/MiscRepositories.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

Status AuditRepository::log(qint64 userId, const QString &action, const QString &entity, qint64 entityId,
                            const QString &details)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO audit_logs (user_id, action, entity, entity_id, details, created_at) "
        "VALUES (:u, :a, :e, :id, :d, :t)"));
    q.bindValue(QStringLiteral(":u"), sql::nullable(userId));
    q.bindValue(QStringLiteral(":a"), action);
    q.bindValue(QStringLiteral(":e"), entity);
    q.bindValue(QStringLiteral(":id"), sql::nullable(entityId));
    q.bindValue(QStringLiteral(":d"), sql::text(details));
    q.bindValue(QStringLiteral(":t"), sql::nowUtc());
    return sql::exec(q, "AuditRepository::log");
}

Result<QString> SettingsRepository::get(const QString &key, const QString &defaultValue) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = :k"));
    q.bindValue(QStringLiteral(":k"), key);
    if (auto st = sql::exec(q, "SettingsRepository::get"); !st)
        return st;
    return q.next() ? q.value(0).toString() : defaultValue;
}

Status SettingsRepository::set(const QString &key, const QString &value)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO app_settings (key, value) VALUES (:k, :v) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.bindValue(QStringLiteral(":k"), key);
    q.bindValue(QStringLiteral(":v"), value);
    return sql::exec(q, "SettingsRepository::set");
}

Result<std::optional<Address>> AddressRepository::defaultFor(qint64 userId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, user_id, full_name, phone, line1, city FROM addresses "
        "WHERE user_id = :u AND is_default = 1 ORDER BY id DESC LIMIT 1"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (auto st = sql::exec(q, "AddressRepository::defaultFor"); !st)
        return st;
    if (!q.next())
        return std::optional<Address>{};
    Address a;
    a.id = q.value(0).toLongLong();
    a.userId = q.value(1).toLongLong();
    a.fullName = q.value(2).toString();
    a.phone = q.value(3).toString();
    a.line1 = q.value(4).toString();
    a.city = q.value(5).toString();
    return std::optional<Address>{a};
}

Status AddressRepository::saveDefault(const Address &a)
{
    auto existing = defaultFor(a.userId);
    if (!existing)
        return existing.status();

    const QString now = sql::nowUtc();
    QSqlQuery q(db());
    if (existing.value()) {
        q.prepare(QStringLiteral(
            "UPDATE addresses SET full_name = :n, phone = :p, line1 = :l, city = :c, updated_at = :u "
            "WHERE id = :id"));
        q.bindValue(QStringLiteral(":id"), existing.value()->id);
    } else {
        q.prepare(QStringLiteral(
            "INSERT INTO addresses (user_id, full_name, phone, line1, city, is_default, created_at, updated_at) "
            "VALUES (:user, :n, :p, :l, :c, 1, :u, :u2)"));
        q.bindValue(QStringLiteral(":user"), a.userId);
        q.bindValue(QStringLiteral(":u2"), now);
    }
    q.bindValue(QStringLiteral(":n"), a.fullName);
    q.bindValue(QStringLiteral(":p"), a.phone);
    q.bindValue(QStringLiteral(":l"), a.line1);
    q.bindValue(QStringLiteral(":c"), a.city);
    q.bindValue(QStringLiteral(":u"), now);
    return sql::exec(q, "AddressRepository::saveDefault");
}

} // namespace shophub
