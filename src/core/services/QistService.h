#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

// What an admin types into the plan form.
struct QistPlanInput
{
    QString name;      // e.g. "6 months"
    QistTerms terms;
};

// What a customer may do with qist right now (for the checkout screen).
struct QistEligibility
{
    bool approved = false;   // the shop allows this customer qist
    Money overdue;           // overdue installments; more than zero blocks a new qist
    QVector<QistPlan> plans; // plans on offer
};

// Qist: installments, v2 milestone 3. See docs/QIST.md.
// - The admin creates plans and allows qist per customer and per product.
// - CheckoutService sells on qist (payment method "qist") inside its transaction.
// - Every qist goes into the customer's khata ledger: the qist price when the
//   order is placed, every installment when the admin records it here.
// - OrderService refuses "Delivered" before the down payment is recorded, and
//   takes a cancelled order's qist off the khata.
class QistService
{
public:
    explicit QistService(Repositories &repos);

    // ---- plans
    // Customers only see plans on offer; admins may ask for switched-off ones too.
    Result<QVector<QistPlan>> plans(const Session &session, bool includeInactive = false) const;
    Result<qint64> createPlan(const Session &session, QistPlanInput input);
    // Changing a plan never changes qists that were already sold with it.
    Status updatePlan(const Session &session, qint64 planId, QistPlanInput input);
    Status setPlanActive(const Session &session, qint64 planId, bool active);

    // ---- customers
    Status setApproved(const Session &session, qint64 customerId, bool approved);
    Result<QistEligibility> myEligibility(const Session &session) const;

    // What a qist would cost, without saving anything. downPayment empty = the minimum.
    Result<QistQuote> quote(const Session &session, qint64 planId, Money cashPrice,
                            std::optional<Money> downPayment = std::nullopt) const;

    // ---- qists (with QistAccount::installments filled in for today)
    Result<QVector<QistAccount>> myAccounts(const Session &session) const;
    // Admin. Overdue qists come first.
    Result<QVector<QistAccount>> accounts(const Session &session, const QistQuery &query) const;
    // Admin, or the customer the qist belongs to.
    Result<QistAccount> account(const Session &session, qint64 qistId) const;
    Result<std::optional<QistAccount>> forOrder(const Session &session, qint64 orderId) const;

    // Admin: money received for a qist, applied to the oldest installment first.
    // note = how it was paid ("Cash", "Easypaisa" ...). The ledger entry says which
    // installment(s) it covers. Paying the last rupee completes the qist and marks
    // the order paid. Returns the qist after the payment.
    Result<QistAccount> recordPayment(const Session &session, qint64 qistId, Money amount, const QString &note);

    // ---- helpers for the other services (they run inside the caller's transaction)
    static QDate today(); // the user's local date (follows the sample-data clock)
    static void applyToday(QistAccount &account, const QDate &today);
    // Overdue installments on a customer's active qists.
    static Result<Money> overdueFor(Repositories &repos, qint64 userId, const QDate &today);
    // Still to be collected on active qists, and how many of them are overdue.
    static Result<QPair<Money, int>> outstanding(Repositories &repos, const QDate &today);

private:
    Result<QistAccount> loadForViewer(const Session &session, qint64 qistId) const;
    Status validatePlan(QistPlanInput &input, qint64 excludeId) const;

    Repositories &m_repos;
};

} // namespace shophub
