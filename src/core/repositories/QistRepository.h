#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

#include <QVariantMap>

namespace shophub {

// Qist plans, which customers may buy on qist, and the qists themselves with
// their schedules. See docs/QIST.md. Money received is not stored here: it is
// the sum of the qist's payment entries in the khata ledger (KhataRepository).
class QistRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    // ---- plans
    // Sorted by months, then name. qistCount = qists sold with each plan.
    Result<QVector<QistPlan>> plans(bool includeInactive) const;
    Result<std::optional<QistPlan>> plan(qint64 planId) const;
    Result<bool> planNameExists(const QString &name, qint64 excludeId) const;
    Result<qint64> insertPlan(const QString &name, const QistTerms &terms);
    Status updatePlan(qint64 planId, const QString &name, const QistTerms &terms);
    Status setPlanActive(qint64 planId, bool active);

    // ---- customers (stored on the customer's khata account row)
    Status setApproved(qint64 userId, bool approved);
    Result<bool> isApproved(qint64 userId) const;

    // ---- qists
    // Inserts the qist with the terms, prices and down payment of `account`.
    Result<qint64> insertAccount(const QistAccount &account, qint64 planId);
    Status insertSchedule(qint64 qistId, const QVector<QistInstallment> &schedule);
    Status setState(qint64 qistId, QistState state);

    // Qists with their schedule (QistAccount::schedule) and the money received
    // so far (QistAccount::paid). Newest first. userId 0 = every customer;
    // search matches the customer's name or email, or the order number;
    // states empty = every state.
    Result<QVector<QistAccount>> accounts(qint64 userId, const QString &search,
                                          const QVector<QistState> &states) const;
    Result<std::optional<QistAccount>> findById(qint64 qistId) const;
    Result<std::optional<QistAccount>> findByOrder(qint64 orderId) const;

private:
    Result<QVector<QistAccount>> select(const QString &where, const QVariantMap &binds) const;
};

} // namespace shophub
