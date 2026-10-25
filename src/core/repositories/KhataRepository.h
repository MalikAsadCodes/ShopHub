#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

namespace shophub {

// Credit limits and the append-only khata ledger.
// The balance is always the sum of the entries; balance_after on each entry
// is a running total kept for display and checked against that sum in tests.
// Qist entries live in the same ledger, marked with their qist (docs/QIST.md).
class KhataRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    // Limit, balance and last activity for one customer (empty if no such customer).
    Result<std::optional<KhataAccount>> account(qint64 userId) const;

    // Every customer, with limit and balance. search matches name or email.
    // owingOnly = only customers whose balance is above zero.
    Result<QVector<KhataAccount>> accounts(const QString &search, bool owingOnly) const;

    Status setLimit(qint64 userId, Money limit);
    Result<Money> balance(qint64 userId) const;

    // Appends one entry and returns the new balance (khata and qist together).
    // at = now when invalid. qistId links a qist's entries to that qist (0 = a normal khata entry).
    Result<Money> append(qint64 userId, KhataKind kind, Money amount, qint64 orderId, const QString &note,
                         qint64 createdBy, const QDateTime &at = QDateTime(), qint64 qistId = 0);

    Result<bool> hasEntry(qint64 orderId, KhataKind kind) const;

    // Newest first.
    Result<QVector<KhataEntry>> entries(qint64 userId, int limit) const;

    // Total owed on khata by all customers, and how many customers owe something.
    // Qist entries are left out: they are reported separately (QistService).
    Result<QPair<Money, int>> outstanding() const;
};

} // namespace shophub
