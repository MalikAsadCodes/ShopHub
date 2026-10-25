#include "repositories/QistRepository.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"

#include <QHash>
#include <QSqlQuery>

namespace shophub {

namespace {

QistPlan readPlan(const QSqlQuery &q)
{
    QistPlan p;
    p.id = q.value(0).toLongLong();
    p.name = q.value(1).toString();
    p.terms.months = q.value(2).toInt();
    p.terms.markupBp = q.value(3).toInt();
    p.terms.minDownBp = q.value(4).toInt();
    p.active = q.value(5).toInt() == 1;
    p.qistCount = q.value(6).toInt();
    return p;
}

const QString kSelectPlan = QStringLiteral(
    "SELECT p.id, p.name, p.months, p.markup_bp, p.min_down_bp, p.is_active, "
    "       (SELECT COUNT(*) FROM qist_accounts q WHERE q.plan_id = p.id) "
    "FROM qist_plans p ");

// Paid = the qist's payment entries in the khata ledger (stored as negative amounts).
const QString kSelectAccount = QStringLiteral(
    "SELECT q.id, q.order_id, o.order_number, q.user_id, u.full_name, u.email, q.plan_name, "
    "       q.months, q.markup_bp, q.min_down_bp, q.cash_price_paisa, q.markup_paisa, q.qist_price_paisa, "
    "       q.down_payment_paisa, q.status, q.created_at, "
    "       COALESCE((SELECT -SUM(e.amount_paisa) FROM khata_entries e "
    "                 WHERE e.qist_id = q.id AND e.kind = 'payment'), 0) "
    "FROM qist_accounts q "
    "JOIN orders o ON o.id = q.order_id "
    "JOIN users u ON u.id = q.user_id ");

QistAccount readAccount(const QSqlQuery &q)
{
    QistAccount a;
    a.id = q.value(0).toLongLong();
    a.orderId = q.value(1).toLongLong();
    a.orderNumber = q.value(2).toString();
    a.userId = q.value(3).toLongLong();
    a.customerName = q.value(4).toString();
    a.customerEmail = q.value(5).toString();
    a.planName = q.value(6).toString();
    a.terms.months = q.value(7).toInt();
    a.terms.markupBp = q.value(8).toInt();
    a.terms.minDownBp = q.value(9).toInt();
    a.cashPrice = Money::fromPaisa(q.value(10).toLongLong());
    a.markup = Money::fromPaisa(q.value(11).toLongLong());
    a.qistPrice = Money::fromPaisa(q.value(12).toLongLong());
    a.downPayment = Money::fromPaisa(q.value(13).toLongLong());
    a.state = qistStateFromDb(q.value(14).toString()).value_or(QistState::Active);
    a.createdAt = sql::fromDb(q.value(15));
    a.paid = Money::fromPaisa(q.value(16).toLongLong());
    return a;
}

} // namespace

// ---------------------------------------------------------------- plans

Result<QVector<QistPlan>> QistRepository::plans(bool includeInactive) const
{
    QSqlQuery q(db());
    q.prepare(kSelectPlan + (includeInactive ? QString() : QStringLiteral("WHERE p.is_active = 1 "))
              + QStringLiteral("ORDER BY p.months, p.name COLLATE NOCASE"));
    if (auto st = sql::exec(q, "QistRepository::plans"); !st)
        return st;
    QVector<QistPlan> out;
    while (q.next())
        out.push_back(readPlan(q));
    return out;
}

Result<std::optional<QistPlan>> QistRepository::plan(qint64 planId) const
{
    QSqlQuery q(db());
    q.prepare(kSelectPlan + QStringLiteral("WHERE p.id = :id"));
    q.bindValue(QStringLiteral(":id"), planId);
    if (auto st = sql::exec(q, "QistRepository::plan"); !st)
        return st;
    if (!q.next())
        return std::optional<QistPlan>{};
    return std::optional<QistPlan>{readPlan(q)};
}

Result<bool> QistRepository::planNameExists(const QString &name, qint64 excludeId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM qist_plans WHERE name = :n COLLATE NOCASE AND id <> :id"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":id"), excludeId);
    if (auto st = sql::exec(q, "QistRepository::planNameExists"); !st)
        return st;
    q.next();
    return q.value(0).toInt() > 0;
}

Result<qint64> QistRepository::insertPlan(const QString &name, const QistTerms &terms)
{
    const QString now = sql::nowUtc();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO qist_plans (name, months, markup_bp, min_down_bp, is_active, created_at, updated_at) "
        "VALUES (:n, :m, :mk, :d, 1, :c, :u)"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":m"), terms.months);
    q.bindValue(QStringLiteral(":mk"), terms.markupBp);
    q.bindValue(QStringLiteral(":d"), terms.minDownBp);
    q.bindValue(QStringLiteral(":c"), now);
    q.bindValue(QStringLiteral(":u"), now);
    if (auto st = sql::exec(q, "QistRepository::insertPlan"); !st)
        return st;
    return q.lastInsertId().toLongLong();
}

Status QistRepository::updatePlan(qint64 planId, const QString &name, const QistTerms &terms)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE qist_plans SET name = :n, months = :m, markup_bp = :mk, min_down_bp = :d, updated_at = :u "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":m"), terms.months);
    q.bindValue(QStringLiteral(":mk"), terms.markupBp);
    q.bindValue(QStringLiteral(":d"), terms.minDownBp);
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), planId);
    return sql::exec(q, "QistRepository::updatePlan");
}

Status QistRepository::setPlanActive(qint64 planId, bool active)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE qist_plans SET is_active = :a, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":a"), active ? 1 : 0);
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), planId);
    return sql::exec(q, "QistRepository::setPlanActive");
}

// ---------------------------------------------------------------- customers

Status QistRepository::setApproved(qint64 userId, bool approved)
{
    // A customer without a khata row gets one with no credit limit.
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO khata_accounts (user_id, credit_limit_paisa, qist_approved, created_at, updated_at) "
        "VALUES (:u, 0, :a, :t, :t2) "
        "ON CONFLICT(user_id) DO UPDATE SET qist_approved = excluded.qist_approved, "
        "                                   updated_at = excluded.updated_at"));
    const QString now = sql::nowUtc();
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":a"), approved ? 1 : 0);
    q.bindValue(QStringLiteral(":t"), now);
    q.bindValue(QStringLiteral(":t2"), now);
    return sql::exec(q, "QistRepository::setApproved");
}

Result<bool> QistRepository::isApproved(qint64 userId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COALESCE((SELECT qist_approved FROM khata_accounts WHERE user_id = :u), 0)"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (auto st = sql::exec(q, "QistRepository::isApproved"); !st)
        return st;
    q.next();
    return q.value(0).toInt() == 1;
}

// ---------------------------------------------------------------- qists

Result<qint64> QistRepository::insertAccount(const QistAccount &a, qint64 planId)
{
    const QString now = sql::nowUtc();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO qist_accounts (order_id, user_id, plan_id, plan_name, months, markup_bp, min_down_bp, "
        "  cash_price_paisa, markup_paisa, qist_price_paisa, down_payment_paisa, status, created_at, updated_at) "
        "VALUES (:o, :u, :p, :n, :m, :mk, :d, :cash, :markup, :price, :down, 'active', :c, :c2)"));
    q.bindValue(QStringLiteral(":o"), a.orderId);
    q.bindValue(QStringLiteral(":u"), a.userId);
    q.bindValue(QStringLiteral(":p"), sql::nullable(planId));
    q.bindValue(QStringLiteral(":n"), a.planName);
    q.bindValue(QStringLiteral(":m"), a.terms.months);
    q.bindValue(QStringLiteral(":mk"), a.terms.markupBp);
    q.bindValue(QStringLiteral(":d"), a.terms.minDownBp);
    q.bindValue(QStringLiteral(":cash"), a.cashPrice.paisa());
    q.bindValue(QStringLiteral(":markup"), a.markup.paisa());
    q.bindValue(QStringLiteral(":price"), a.qistPrice.paisa());
    q.bindValue(QStringLiteral(":down"), a.downPayment.paisa());
    q.bindValue(QStringLiteral(":c"), now);
    q.bindValue(QStringLiteral(":c2"), now);
    if (auto st = sql::exec(q, "QistRepository::insertAccount"); !st)
        return st;
    return q.lastInsertId().toLongLong();
}

Status QistRepository::insertSchedule(qint64 qistId, const QVector<QistInstallment> &schedule)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO qist_installments (qist_id, number, due_date, amount_paisa) VALUES (:q, :n, :d, :a)"));
    for (const QistInstallment &line : schedule) {
        q.bindValue(QStringLiteral(":q"), qistId);
        q.bindValue(QStringLiteral(":n"), line.number);
        q.bindValue(QStringLiteral(":d"), line.dueDate.toString(Qt::ISODate));
        q.bindValue(QStringLiteral(":a"), line.amount.paisa());
        if (auto st = sql::exec(q, "QistRepository::insertSchedule"); !st)
            return st;
    }
    return Status::ok();
}

Status QistRepository::setState(qint64 qistId, QistState state)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE qist_accounts SET status = :s, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":s"), qistStateToDb(state));
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), qistId);
    return sql::exec(q, "QistRepository::setState");
}

Result<QVector<QistAccount>> QistRepository::select(const QString &where, const QVariantMap &binds) const
{
    QSqlQuery q(db());
    q.prepare(kSelectAccount + where + QStringLiteral(" ORDER BY q.id DESC"));
    for (auto it = binds.cbegin(); it != binds.cend(); ++it)
        q.bindValue(it.key(), it.value());
    if (auto st = sql::exec(q, "QistRepository::select"); !st)
        return st;
    QVector<QistAccount> out;
    QHash<qint64, int> rowOf;
    QStringList ids;
    while (q.next()) {
        out.push_back(readAccount(q));
        rowOf.insert(out.last().id, int(out.size()) - 1);
        ids << QString::number(out.last().id);
    }
    if (out.isEmpty())
        return out;

    // The schedules of all of them in one query. The ids come from the database, so they are plain numbers.
    QSqlQuery s(db());
    s.prepare(QStringLiteral("SELECT qist_id, number, due_date, amount_paisa FROM qist_installments "
                             "WHERE qist_id IN (%1) ORDER BY qist_id, number")
                  .arg(ids.join(QLatin1Char(','))));
    if (auto st = sql::exec(s, "QistRepository::select(schedule)"); !st)
        return st;
    while (s.next()) {
        const auto row = rowOf.constFind(s.value(0).toLongLong());
        if (row == rowOf.constEnd())
            continue;
        QistInstallment line;
        line.number = s.value(1).toInt();
        line.dueDate = QDate::fromString(s.value(2).toString(), Qt::ISODate);
        line.amount = Money::fromPaisa(s.value(3).toLongLong());
        out[row.value()].schedule.push_back(line);
    }
    return out;
}

Result<QVector<QistAccount>> QistRepository::accounts(qint64 userId, const QString &search,
                                                      const QVector<QistState> &states) const
{
    QStringList where;
    QVariantMap binds;
    if (userId > 0) {
        where << QStringLiteral("q.user_id = :uid");
        binds[QStringLiteral(":uid")] = userId;
    }
    const QString text = search.trimmed();
    if (!text.isEmpty()) {
        const QString like = QLatin1Char('%') + validation::escapeLike(text) + QLatin1Char('%');
        where << QStringLiteral("(u.full_name LIKE :s1 ESCAPE '\\' OR u.email LIKE :s2 ESCAPE '\\' "
                                "OR o.order_number LIKE :s3 ESCAPE '\\')");
        binds[QStringLiteral(":s1")] = like;
        binds[QStringLiteral(":s2")] = like;
        binds[QStringLiteral(":s3")] = like;
    }
    if (!states.isEmpty()) {
        QStringList names;
        for (QistState s : states)
            names << QLatin1Char('\'') + qistStateToDb(s) + QLatin1Char('\''); // fixed words, never user input
        where << QStringLiteral("q.status IN (%1)").arg(names.join(QLatin1Char(',')));
    }
    const QString whereSql = where.isEmpty() ? QString() : QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND "));
    return select(whereSql, binds);
}

Result<std::optional<QistAccount>> QistRepository::findById(qint64 qistId) const
{
    QVariantMap binds;
    binds.insert(QStringLiteral(":id"), qistId);
    auto found = select(QStringLiteral("WHERE q.id = :id"), binds);
    if (!found)
        return found.status();
    if (found.value().isEmpty())
        return std::optional<QistAccount>{};
    return std::optional<QistAccount>{found.value().first()};
}

Result<std::optional<QistAccount>> QistRepository::findByOrder(qint64 orderId) const
{
    QVariantMap binds;
    binds.insert(QStringLiteral(":id"), orderId);
    auto found = select(QStringLiteral("WHERE q.order_id = :id"), binds);
    if (!found)
        return found.status();
    if (found.value().isEmpty())
        return std::optional<QistAccount>{};
    return std::optional<QistAccount>{found.value().first()};
}

} // namespace shophub
