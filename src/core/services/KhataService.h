#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

// Khata: the shop's credit book.
// - The admin gives a customer a credit limit (0 = no credit).
// - The customer can then choose "Khata" at checkout; CheckoutService adds the
//   order to the ledger inside the same transaction as the order itself.
// - Cancelling a khata order adds a reversal (OrderService).
// - The admin records payments and adjustments here.
// Entries are never changed or deleted; mistakes are fixed with an adjustment.
class KhataService
{
public:
    static constexpr qint64 kMaxLimitRupees = 10'000'000;   // Rs 1 crore
    static constexpr qint64 kMaxEntryRupees = 10'000'000;

    explicit KhataService(Repositories &repos);

    // ---- customer
    Result<KhataAccount> myAccount(const Session &session) const;
    Result<QVector<KhataEntry>> myEntries(const Session &session, int limit = 500) const;

    // ---- admin
    Result<QVector<KhataAccount>> accounts(const Session &session, const QString &search = QString(),
                                           bool owingOnly = false) const;
    Result<KhataAccount> account(const Session &session, qint64 customerId) const;
    Result<QVector<KhataEntry>> entries(const Session &session, qint64 customerId, int limit = 500) const;

    Status setLimit(const Session &session, qint64 customerId, Money limit);

    // Money received from the customer. Cannot be more than what they owe.
    // note = how it was paid, e.g. "Cash", "Easypaisa". Returns the new balance.
    Result<Money> recordPayment(const Session &session, qint64 customerId, Money amount, const QString &note);

    // A correction by the shop. Positive raises the balance, negative lowers it
    // (for example a write-off). A note explaining why is required.
    Result<Money> adjust(const Session &session, qint64 customerId, Money signedAmount, const QString &note);

private:
    Result<KhataAccount> loadCustomer(qint64 customerId) const;

    Repositories &m_repos;
};

} // namespace shophub
