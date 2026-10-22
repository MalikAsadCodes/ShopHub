#include "repositories/KhataRepository.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

namespace {

// One row per customer: id, name, email, limit, balance, last activity.
const QString kSelectAccount = QStringLiteral(
    "SELECT u.id, u.full_name, u.email, COALESCE(a.credit_limit_paisa, 0), "
    "       COALESCE((SELECT SUM(e.amount_paisa) FROM khata_entries e WHERE e.user_id = u.id), 0) AS bal, "
    "       (SELECT MAX(e.created_at) FROM khata_entries e WHERE e.user_id = u.id) "
    "FROM users u LEFT JOIN khata_accounts a ON a.user_id = u.id "
    "WHERE u.role = 'customer' ");

KhataAccount readAccount(const QSqlQuery &q)
{
    KhataAccount a;
    a.userId = q.value(0).toLongLong();
    a.customerName = q.value(1).toString();
    a.email = q.value(2).toString();
    a.creditLimit = Money::fromPaisa(q.value(3).toLongLong());
    a.balance = Money::fromPaisa(q.value(4).toLongLong());
    a.lastActivity = sql::fromDb(q.value(5));
    return a;
}

} // namespace

Result<std::optional<KhataAccount>> KhataRepository::account(qint64 userId) const
{
    QSqlQuery q(db());
    q.prepare(kSelectAccount + QStringLiteral("AND u.id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    if (auto st = sql::exec(q, "KhataRepository::account"); !st)
        return st;
    if (!q.next())
        return std::optional<KhataAccount>{};
    return std::optional<KhataAccount>{readAccount(q)};
}

Result<QVector<KhataAccount>> KhataRepository::accounts(const QString &search, bool owingOnly) const
{
    QString where;
    const QString text = search.trimmed();
    if (!text.isEmpty())
        where += QStringLiteral("AND (u.full_name LIKE :s1 ESCAPE '\\' OR u.email LIKE :s2 ESCAPE '\\') ");

    // Wrapped so the balance alias can be filtered and sorted on.
    QString sqlText = QStringLiteral("SELECT * FROM (") + kSelectAccount + where + QStringLiteral(") t ");
    if (owingOnly)
        sqlText += QStringLiteral("WHERE t.bal > 0 ");
    sqlText += QStringLiteral("ORDER BY t.bal DESC, t.full_name COLLATE NOCASE");

    QSqlQuery q(db());
    q.prepare(sqlText);
    if (!text.isEmpty()) {
        const QString like = QLatin1Char('%') + validation::escapeLike(text) + QLatin1Char('%');
        q.bindValue(QStringLiteral(":s1"), like);
        q.bindValue(QStringLiteral(":s2"), like);
    }
    if (auto st = sql::exec(q, "KhataRepository::accounts"); !st)
        return st;
    QVector<KhataAccount> out;
    while (q.next())
        out.push_back(readAccount(q));
    return out;
}

Status KhataRepository::setLimit(qint64 userId, Money limit)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO khata_accounts (user_id, credit_limit_paisa, created_at, updated_at) "
        "VALUES (:u, :l, :t, :t2) "
        "ON CONFLICT(user_id) DO UPDATE SET credit_limit_paisa = excluded.credit_limit_paisa, "
        "                                   updated_at = excluded.updated_at"));
    const QString now = sql::nowUtc();
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":l"), limit.paisa());
    q.bindValue(QStringLiteral(":t"), now);
    q.bindValue(QStringLiteral(":t2"), now);
    return sql::exec(q, "KhataRepository::setLimit");
}

Result<Money> KhataRepository::balance(qint64 userId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COALESCE(SUM(amount_paisa), 0) FROM khata_entries WHERE user_id = :u"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (auto st = sql::exec(q, "KhataRepository::balance"); !st)
        return st;
    q.next();
    return Money::fromPaisa(q.value(0).toLongLong());
}

Result<Money> KhataRepository::append(qint64 userId, KhataKind kind, Money amount, qint64 orderId,
                                      const QString &note, qint64 createdBy, const QDateTime &at)
{
    auto current = balance(userId);
    if (!current)
        return current;
    const Money after = current.value() + amount;

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO khata_entries (user_id, kind, amount_paisa, balance_after_paisa, order_id, note, "
        "  created_by, created_at) VALUES (:u, :k, :a, :b, :o, :n, :by, :at)"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":k"), khataKindToDb(kind));
    q.bindValue(QStringLiteral(":a"), amount.paisa());
    q.bindValue(QStringLiteral(":b"), after.paisa());
    q.bindValue(QStringLiteral(":o"), sql::nullable(orderId));
    q.bindValue(QStringLiteral(":n"), sql::text(note));
    q.bindValue(QStringLiteral(":by"), sql::nullable(createdBy));
    q.bindValue(QStringLiteral(":at"), at.isValid() ? sql::toDb(at) : sql::nowUtc());
    if (auto st = sql::exec(q, "KhataRepository::append"); !st)
        return st;
    return after;
}

Result<bool> KhataRepository::hasEntry(qint64 orderId, KhataKind kind) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE order_id = :o AND kind = :k"));
    q.bindValue(QStringLiteral(":o"), orderId);
    q.bindValue(QStringLiteral(":k"), khataKindToDb(kind));
    if (auto st = sql::exec(q, "KhataRepository::hasEntry"); !st)
        return st;
    q.next();
    return q.value(0).toInt() > 0;
}

Result<QVector<KhataEntry>> KhataRepository::entries(qint64 userId, int limit) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT e.id, e.user_id, e.kind, e.amount_paisa, e.balance_after_paisa, COALESCE(e.order_id, 0), "
        "       COALESCE(o.order_number, ''), e.note, COALESCE(u.full_name, ''), e.created_at "
        "FROM khata_entries e "
        "LEFT JOIN orders o ON o.id = e.order_id "
        "LEFT JOIN users u ON u.id = e.created_by "
        "WHERE e.user_id = :u ORDER BY e.id DESC LIMIT :lim"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":lim"), qBound(1, limit, 5000));
    if (auto st = sql::exec(q, "KhataRepository::entries"); !st)
        return st;
    QVector<KhataEntry> out;
    while (q.next()) {
        KhataEntry e;
        e.id = q.value(0).toLongLong();
        e.userId = q.value(1).toLongLong();
        e.kind = khataKindFromDb(q.value(2).toString()).value_or(KhataKind::Adjustment);
        e.amount = Money::fromPaisa(q.value(3).toLongLong());
        e.balanceAfter = Money::fromPaisa(q.value(4).toLongLong());
        e.orderId = q.value(5).toLongLong();
        e.orderNumber = q.value(6).toString();
        e.note = q.value(7).toString();
        e.createdBy = q.value(8).toString();
        e.createdAt = sql::fromDb(q.value(9));
        out.push_back(e);
    }
    return out;
}

Result<QPair<Money, int>> KhataRepository::outstanding() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(bal), 0), COUNT(*) FROM "
        "(SELECT SUM(amount_paisa) AS bal FROM khata_entries GROUP BY user_id) WHERE bal > 0"));
    if (auto st = sql::exec(q, "KhataRepository::outstanding"); !st)
        return st;
    q.next();
    return qMakePair(Money::fromPaisa(q.value(0).toLongLong()), q.value(1).toInt());
}

} // namespace shophub
