#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

// Returns after delivery, ShopHub v3 milestone 1. See docs/RETURNS.md.
// - A customer asks to return some or all of a delivered order, within the
//   shop's return window (7 days after delivery unless the shop changes it),
//   with a reason and an optional note.
// - The shop approves or rejects the request (a rejection needs a reason the
//   customer sees). The shop can also open a return itself, for example to
//   accept one after the window; it starts approved.
// - The customer can cancel a request until the shop decides; the shop can
//   cancel a request, or an approved return whose goods never came back.
// - Milestone 2: receiving the goods (back to stock or damaged) and settling
//   the money (paid back, khata credit, or off the qist), with a credit note
//   for the returned part.
class ReturnService
{
public:
    explicit ReturnService(Repositories &repos);

    // ---- settings (admin)
    Result<ReturnSettings> settings(const Session &session) const;
    // The same policy for anyone signed in: customers see the return window in the shop.
    Result<ReturnSettings> policy(const Session &session) const;
    Status saveSettings(const Session &session, const ReturnSettings &settings);

    // ---- asking (the order's customer, or an admin)
    // What can be returned from this order, and whether the signed-in user may open a return now.
    Result<ReturnEligibility> eligibility(const Session &session, qint64 orderId) const;
    // A customer's request starts "requested"; one opened by an admin starts "approved".
    Result<ReturnRequest> request(const Session &session, qint64 orderId, const ReturnInput &input);

    // ---- deciding (admin)
    Status approve(const Session &session, qint64 returnId, const QString &note = QString());
    Status reject(const Session &session, qint64 returnId, const QString &reason);
    // The customer (own request, still waiting) or an admin (waiting or approved).
    Status cancel(const Session &session, qint64 returnId, const QString &note = QString());

    // ---- receiving (admin, v3 milestone 2)
    // What receiving an approved return would credit and how, without saving anything.
    Result<RefundPreview> previewReceive(const Session &session, qint64 returnId, const ReceiveInput &input) const;
    // The goods are back: each unit to stock or damaged, the money settled
    // (paid back, khata credit, or off the qist), a credit note for the
    // returned part if the order was invoiced. All in one transaction; the
    // return ends Completed.
    Result<ReturnRequest> receive(const Session &session, qint64 returnId, const ReceiveInput &input);

    // ---- reading
    Result<QVector<ReturnRequest>> list(const Session &session, const ReturnQuery &query) const; // admin
    Result<QVector<ReturnRequest>> forOrder(const Session &session, qint64 orderId) const;      // with lines
    Result<ReturnRequest> details(const Session &session, qint64 returnId) const;               // with lines
    Result<int> waitingCount(const Session &session) const; // admin: requests waiting for review

private:
    Result<Order> loadOrder(const Session &session, qint64 orderId) const;
    Result<ReturnRequest> loadReturn(const Session &session, qint64 returnId) const;
    Result<ReturnSettings> readSettings() const;
    Status decide(const Session &session, qint64 returnId, ReturnStatus to, const QString &note, const char *action);
    Result<RefundPreview> computeRefund(const ReturnRequest &request, const Order &order, const ReceiveInput &input,
                                        std::optional<QistAccount> &qist) const;

    Repositories &m_repos;
};

} // namespace shophub
