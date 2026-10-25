#include "services/QistService.h"

#include "database/SqlUtil.h"
#include "database/Transaction.h"
#include "services/Access.h"
#include "services/KhataService.h"

#include <QtDebug>

#include <algorithm>

namespace shophub {

namespace {

// "6 months: 10% markup, 20% down" for the audit log.
QString describeTerms(const QString &name, const QistTerms &t)
{
    return QStringLiteral("%1: %2 months, %3% markup, %4% down")
        .arg(name, QString::number(t.months), pricetier::percentText(t.markupBp), pricetier::percentText(t.minDownBp));
}

} // namespace

QistService::QistService(Repositories &repos)
    : m_repos(repos)
{
}

// ---------------------------------------------------------------- helpers

QDate QistService::today()
{
    return sql::todayLocal();
}

void QistService::applyToday(QistAccount &account, const QDate &today)
{
    account.installments = qist::applyPayments(account.schedule, account.paid, today);
}

Result<Money> QistService::overdueFor(Repositories &repos, qint64 userId, const QDate &today)
{
    auto active = repos.qist.accounts(userId, QString(), {QistState::Active});
    if (!active)
        return active.status();
    Money overdue;
    for (QistAccount &a : active.value()) {
        applyToday(a, today);
        overdue += a.overdueAmount();
    }
    return overdue;
}

Result<QPair<Money, int>> QistService::outstanding(Repositories &repos, const QDate &today)
{
    auto active = repos.qist.accounts(0, QString(), {QistState::Active});
    if (!active)
        return active.status();
    Money left;
    int overdue = 0;
    for (QistAccount &a : active.value()) {
        applyToday(a, today);
        left += a.left();
        if (a.isOverdue())
            ++overdue;
    }
    return qMakePair(left, overdue);
}

// ---------------------------------------------------------------- plans

Result<QVector<QistPlan>> QistService::plans(const Session &session, bool includeInactive) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    return m_repos.qist.plans(includeInactive && session.isAdmin());
}

Status QistService::validatePlan(QistPlanInput &input, qint64 excludeId) const
{
    input.name = input.name.simplified();
    if (input.name.isEmpty())
        return Status::fail(QStringLiteral("Give the plan a name, for example \"6 months\"."));
    if (input.name.size() > 60)
        return Status::fail(QStringLiteral("The plan name can be at most 60 characters."));
    if (const QString error = qist::validateTerms(input.terms); !error.isEmpty())
        return Status::fail(error);
    auto exists = m_repos.qist.planNameExists(input.name, excludeId);
    if (!exists)
        return exists.status();
    if (exists.value())
        return Status::fail(QStringLiteral("There is already a plan called \"%1\".").arg(input.name));
    return Status::ok();
}

Result<qint64> QistService::createPlan(const Session &session, QistPlanInput input)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    if (auto st = validatePlan(input, 0); !st)
        return st;
    auto id = m_repos.qist.insertPlan(input.name, input.terms);
    if (!id)
        return id;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("qist_plan_created"), QStringLiteral("qist_plan"),
                                    id.value(), describeTerms(input.name, input.terms));
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    return id;
}

Status QistService::updatePlan(const Session &session, qint64 planId, QistPlanInput input)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto existing = m_repos.qist.plan(planId);
    if (!existing)
        return existing.status();
    if (!existing.value())
        return Status::fail(QStringLiteral("Plan not found."));
    if (auto st = validatePlan(input, planId); !st)
        return st;
    if (auto st = m_repos.qist.updatePlan(planId, input.name, input.terms); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("qist_plan_updated"), QStringLiteral("qist_plan"),
                                    planId, describeTerms(input.name, input.terms));
        !st)
        return st;
    return tx.commit();
}

Status QistService::setPlanActive(const Session &session, qint64 planId, bool active)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto existing = m_repos.qist.plan(planId);
    if (!existing)
        return existing.status();
    if (!existing.value())
        return Status::fail(QStringLiteral("Plan not found."));
    if (auto st = m_repos.qist.setPlanActive(planId, active); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(),
                                    active ? QStringLiteral("qist_plan_on") : QStringLiteral("qist_plan_off"),
                                    QStringLiteral("qist_plan"), planId, existing.value()->name);
        !st)
        return st;
    return tx.commit();
}

// ---------------------------------------------------------------- customers

Status QistService::setApproved(const Session &session, qint64 customerId, bool approved)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto customer = m_repos.khata.account(customerId); // customers only
    if (!customer)
        return customer.status();
    if (!customer.value())
        return Status::fail(QStringLiteral("Customer not found."));
    if (auto st = m_repos.qist.setApproved(customerId, approved); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(),
                                    approved ? QStringLiteral("qist_allowed") : QStringLiteral("qist_stopped"),
                                    QStringLiteral("user"), customerId);
        !st)
        return st;
    return tx.commit();
}

Result<QistEligibility> QistService::myEligibility(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    QistEligibility e;
    auto approved = m_repos.qist.isApproved(session.userId());
    if (!approved)
        return approved.status();
    e.approved = approved.value();
    auto overdue = overdueFor(m_repos, session.userId(), today());
    if (!overdue)
        return overdue.status();
    e.overdue = overdue.value();
    auto plans = m_repos.qist.plans(false);
    if (!plans)
        return plans.status();
    e.plans = plans.value();
    return e;
}

Result<QistQuote> QistService::quote(const Session &session, qint64 planId, Money cashPrice,
                                     std::optional<Money> downPayment) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto plan = m_repos.qist.plan(planId);
    if (!plan)
        return plan.status();
    if (!plan.value() || (!plan.value()->active && !session.isAdmin()))
        return Result<QistQuote>::fail(QStringLiteral("Please choose a qist plan."));
    return qist::quote(plan.value()->terms, cashPrice, downPayment, today());
}

// ---------------------------------------------------------------- qists

Result<QVector<QistAccount>> QistService::myAccounts(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    auto found = m_repos.qist.accounts(session.userId(), QString(), {});
    if (!found)
        return found.status();
    const QDate day = today();
    QVector<QistAccount> out = found.value();
    for (QistAccount &a : out)
        applyToday(a, day);
    return out;
}

Result<QVector<QistAccount>> QistService::accounts(const Session &session, const QistQuery &query) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    QVector<QistState> states;
    switch (query.show) {
    case QistQuery::Show::Open:
    case QistQuery::Show::Overdue: states = {QistState::Active}; break;
    case QistQuery::Show::Completed: states = {QistState::Completed}; break;
    case QistQuery::Show::Cancelled: states = {QistState::Cancelled}; break;
    case QistQuery::Show::All: break;
    }
    auto found = m_repos.qist.accounts(query.userId, query.search, states);
    if (!found)
        return found.status();

    const QDate day = today();
    QVector<QistAccount> out;
    for (QistAccount a : found.value()) {
        applyToday(a, day);
        if (query.show == QistQuery::Show::Overdue && !a.isOverdue())
            continue;
        out.push_back(a);
    }
    // Overdue first (the longest overdue at the top), then newest first as loaded.
    std::stable_sort(out.begin(), out.end(), [&day](const QistAccount &x, const QistAccount &y) {
        const int dx = x.isOverdue() ? qist::daysOverdue(x.installments, day) : -1;
        const int dy = y.isOverdue() ? qist::daysOverdue(y.installments, day) : -1;
        return dx > dy;
    });
    return out;
}

Result<QistAccount> QistService::loadForViewer(const Session &session, qint64 qistId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.qist.findById(qistId);
    if (!found)
        return found.status();
    // Same message for "missing" and "someone else's".
    if (!found.value() || (!session.isAdmin() && found.value()->userId != session.userId()))
        return Result<QistAccount>::fail(QStringLiteral("Qist not found."));
    QistAccount a = *found.value();
    applyToday(a, today());
    return a;
}

Result<QistAccount> QistService::account(const Session &session, qint64 qistId) const
{
    return loadForViewer(session, qistId);
}

Result<std::optional<QistAccount>> QistService::forOrder(const Session &session, qint64 orderId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.qist.findByOrder(orderId);
    if (!found)
        return found.status();
    if (!found.value() || (!session.isAdmin() && found.value()->userId != session.userId()))
        return std::optional<QistAccount>{};
    QistAccount a = *found.value();
    applyToday(a, today());
    return std::optional<QistAccount>{a};
}

Result<QistAccount> QistService::recordPayment(const Session &session, qint64 qistId, Money amount,
                                               const QString &note)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (amount.isNegative() || amount.isZero())
        return Result<QistAccount>::fail(QStringLiteral("Enter the amount received."));
    if (amount > Money::fromRupees(KhataService::kMaxEntryRupees))
        return Result<QistAccount>::fail(QStringLiteral("That amount is too large."));
    if (note.trimmed().size() > 200)
        return Result<QistAccount>::fail(QStringLiteral("The note can be at most 200 characters."));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto found = m_repos.qist.findById(qistId);
    if (!found)
        return found.status();
    if (!found.value())
        return Result<QistAccount>::fail(QStringLiteral("Qist not found."));
    const QistAccount a = *found.value();
    if (a.state == QistState::Cancelled)
        return Result<QistAccount>::fail(QStringLiteral("This qist was cancelled. There is nothing to pay."));
    if (a.state == QistState::Completed)
        return Result<QistAccount>::fail(QStringLiteral("This qist is already paid in full."));
    const Money left = a.qistPrice - a.paid;
    if (amount > left)
        return Result<QistAccount>::fail(
            QStringLiteral("Only %1 is left to pay on this qist. The payment cannot be more than that.")
                .arg(left.toString()));

    QString text = qist::describePayment(a.schedule, a.paid, amount);
    if (!note.trimmed().isEmpty())
        text += QStringLiteral(" - ") + note.trimmed();
    auto after = m_repos.khata.append(a.userId, KhataKind::Payment, Money() - amount, a.orderId, text,
                                      session.userId(), QDateTime(), a.id);
    if (!after)
        return after.status();

    const bool finished = amount == left;
    if (finished) {
        if (auto st = m_repos.qist.setState(a.id, QistState::Completed); !st)
            return st;
        if (auto st = m_repos.orders.setPaymentStatus(a.orderId, payment::kPaid); !st)
            return st;
    }
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("qist_payment"), QStringLiteral("qist"), a.id,
                                    QStringLiteral("%1 %2").arg(a.orderNumber, amount.toPlainString()));
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    qInfo().noquote() << "Qist payment recorded for" << a.orderNumber << (finished ? "(completed)" : "");
    return loadForViewer(session, qistId);
}

} // namespace shophub
