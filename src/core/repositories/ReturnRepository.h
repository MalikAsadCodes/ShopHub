#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

#include <QHash>
#include <QVariantMap>

namespace shophub {

// Returns after delivery and their lines (migration 7). See docs/RETURNS.md.
// The database refuses status changes outside the allowed steps, more units
// than were bought, edits to what was asked, and deletes.
class ReturnRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    // Inserts the header and its lines. Returns the new id.
    Result<qint64> insert(const ReturnRequest &request, qint64 requestedBy);

    Result<std::optional<ReturnRequest>> findById(qint64 id) const;     // with lines
    Result<QVector<ReturnRequest>> forOrder(qint64 orderId) const;      // with lines, newest first
    Result<QVector<ReturnRequest>> list(const ReturnQuery &query) const; // without lines, open ones first

    // Units of each order line in returns that are not rejected or cancelled.
    Result<QHash<qint64, int>> returnedUnits(qint64 orderId) const;

    // Moves a return to a new status and records who decided and why.
    Status setStatus(qint64 id, ReturnStatus status, qint64 decidedBy, const QString &decisionNote);

    Result<int> countWithStatus(ReturnStatus status) const;

    // ---- receiving (v3 milestone 2)
    // Moves the status only; who approved it and why stay as they were.
    Status setStatusOnly(qint64 id, ReturnStatus status);
    Status insertReceipt(qint64 returnId, const ReturnLine &line);   // restocked, damaged, value, creditedTax
    Status insertRefund(qint64 returnId, const ReturnRefund &refund, qint64 receivedBy);

    // What earlier received returns of this order already credited, per order line.
    struct Credited
    {
        int units = 0;
        Money tax;
    };
    Result<QHash<qint64, Credited>> creditedLines(qint64 orderId) const;
    // Delivery charge already refunded for this order, and the qist markup earlier returns took off.
    Result<QPair<Money, Money>> refundedForOrder(qint64 orderId) const;

private:
    Result<QVector<ReturnRequest>> select(const QString &where, const QVariantMap &binds, bool withLines,
                                          const QString &tail) const;
    Status loadLines(ReturnRequest &request) const;
};

} // namespace shophub
