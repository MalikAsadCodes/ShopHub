#include "repositories/UserRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

namespace {

const QString kUserColumns = QStringLiteral(
    "id, email, full_name, role, is_active, created_at, password_hash, failed_logins, locked_until");

User readUser(const QSqlQuery &q)
{
    User u;
    u.id = q.value(0).toLongLong();
    u.email = q.value(1).toString();
    u.fullName = q.value(2).toString();
    u.role = roleFromDb(q.value(3).toString()).value_or(Role::Customer);
    u.active = q.value(4).toInt() == 1;
    u.createdAt = sql::fromDb(q.value(5));
    return u;
}

} // namespace

Result<std::optional<UserRecord>> UserRepository::findByEmail(const QString &email) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT %1 FROM users WHERE email = :e").arg(kUserColumns));
    q.bindValue(QStringLiteral(":e"), email);
    if (auto st = sql::exec(q, "UserRepository::findByEmail"); !st)
        return st;
    if (!q.next())
        return std::optional<UserRecord>{};

    UserRecord r;
    r.user = readUser(q);
    r.passwordHash = q.value(6).toString();
    r.failedLogins = q.value(7).toInt();
    r.lockedUntil = sql::fromDb(q.value(8));
    return std::optional<UserRecord>{r};
}

Result<std::optional<User>> UserRepository::findById(qint64 id) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT %1 FROM users WHERE id = :id").arg(kUserColumns));
    q.bindValue(QStringLiteral(":id"), id);
    if (auto st = sql::exec(q, "UserRepository::findById"); !st)
        return st;
    if (!q.next())
        return std::optional<User>{};
    return std::optional<User>{readUser(q)};
}

Result<qint64> UserRepository::insert(const QString &email, const QString &fullName,
                                      const QString &passwordHash, Role role)
{
    const QString now = sql::nowUtc();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO users (email, full_name, password_hash, role, created_at, updated_at) "
        "VALUES (:e, :n, :h, :r, :c, :u)"));
    q.bindValue(QStringLiteral(":e"), email);
    q.bindValue(QStringLiteral(":n"), fullName);
    q.bindValue(QStringLiteral(":h"), passwordHash);
    q.bindValue(QStringLiteral(":r"), roleToDb(role));
    q.bindValue(QStringLiteral(":c"), now);
    q.bindValue(QStringLiteral(":u"), now);
    if (auto st = sql::exec(q, "UserRepository::insert"); !st)
        return st;
    return q.lastInsertId().toLongLong();
}

Status UserRepository::recordFailedLogin(qint64 id, int failedLogins, const QDateTime &lockedUntil)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE users SET failed_logins = :f, locked_until = :l, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":f"), failedLogins);
    q.bindValue(QStringLiteral(":l"), lockedUntil.isValid() ? QVariant(sql::toDb(lockedUntil))
                                                            : QVariant(QMetaType(QMetaType::QString)));
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "UserRepository::recordFailedLogin");
}

Status UserRepository::recordSuccessfulLogin(qint64 id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE users SET failed_logins = 0, locked_until = NULL, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "UserRepository::recordSuccessfulLogin");
}

Status UserRepository::updatePasswordHash(qint64 id, const QString &passwordHash)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE users SET password_hash = :h, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":h"), passwordHash);
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "UserRepository::updatePasswordHash");
}

Result<int> UserRepository::countByRole(Role role) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM users WHERE role = :r"));
    q.bindValue(QStringLiteral(":r"), roleToDb(role));
    if (auto st = sql::exec(q, "UserRepository::countByRole"); !st)
        return st;
    q.next();
    return q.value(0).toInt();
}

} // namespace shophub
