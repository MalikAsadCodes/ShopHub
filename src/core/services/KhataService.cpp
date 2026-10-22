#include "services/KhataService.h"

#include "database/Transaction.h"
#include "services/Access.h"

#include <QtDebug>

namespace shophub {

KhataService::KhataService(Repositories &repos)
    : m_repos(repos)
{
}

Result<KhataAccount> KhataService::loadCustomer(qint64 customerId) const
{
    auto found = m_repos.khata.account(customerId);
    if (!found)
        return found.status();
    if (!found.value())
        return Result<KhataAccount>::fail(QStringLiteral("Customer not found."));
    return *found.value();
}

Result<KhataAccount> KhataService::myAccount(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return loadCustomer(session.userId());
}

Result<QVector<KhataEntry>> KhataService::myEntries(const Session &session, int limit) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return m_repos.khata.entries(session.userId(), limit);
}

Result<QVector<KhataAccount>> KhataService::accounts(const Session &session, const QString &search,
                                                     bool owingOnly) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return m_repos.khata.accounts(search, owingOnly);
}

Result<KhataAccount> KhataService::account(const Session &session, qint64 customerId) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return loadCustomer(customerId);
}

Result<QVector<KhataEntry>> KhataService::entries(const Session &session, qint64 customerId, int limit) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (auto customer = loadCustomer(customerId); !customer)
        return customer.status();
    return m_repos.khata.entries(customerId, limit);
}

Status KhataService::setLimit(const Session &session, qint64 customerId, Money limit)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (limit.isNegative())
        return Status::fail(QStringLiteral("The credit limit cannot be negative."));
    if (limit > Money::fromRupees(kMaxLimitRupees))
        return Status::fail(QStringLiteral("The credit limit can be at most %1.")
                                .arg(Money::fromRupees(kMaxLimitRupees).toString()));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto customer = loadCustomer(customerId);
    if (!customer)
        return customer.status();
    if (auto st = m_repos.khata.setLimit(customerId, limit); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("khata_limit_set"), QStringLiteral("user"),
                                    customerId,
                                    QStringLiteral("%1 -> %2").arg(customer.value().creditLimit.toPlainString(),
                                                                   limit.toPlainString()));
        !st)
        return st;
    return tx.commit();
}

Result<Money> KhataService::recordPayment(const Session &session, qint64 customerId, Money amount,
                                          const QString &note)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (amount.isNegative() || amount.isZero())
        return Result<Money>::fail(QStringLiteral("Enter the amount received."));
    if (amount > Money::fromRupees(kMaxEntryRupees))
        return Result<Money>::fail(QStringLiteral("That amount is too large."));
    if (note.trimmed().size() > 200)
        return Result<Money>::fail(QStringLiteral("The note can be at most 200 characters."));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto customer = loadCustomer(customerId);
    if (!customer)
        return customer.status();
    const Money owed = customer.value().balance;
    if (owed <= Money())
        return Result<Money>::fail(QStringLiteral("%1 does not owe anything.").arg(customer.value().customerName));
    if (amount > owed)
        return Result<Money>::fail(QStringLiteral("%1 owes only %2. The payment cannot be more than that.")
                                       .arg(customer.value().customerName, owed.toString()));

    auto after = m_repos.khata.append(customerId, KhataKind::Payment, Money() - amount, 0, note.trimmed(),
                                      session.userId());
    if (!after)
        return after;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("khata_payment"), QStringLiteral("user"),
                                    customerId, amount.toPlainString());
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    qInfo().noquote() << "Khata payment recorded, new balance" << after.value().toString();
    return after;
}

Result<Money> KhataService::adjust(const Session &session, qint64 customerId, Money signedAmount,
                                   const QString &note)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (signedAmount.isZero())
        return Result<Money>::fail(QStringLiteral("The adjustment cannot be zero."));
    const Money size = signedAmount.isNegative() ? Money() - signedAmount : signedAmount;
    if (size > Money::fromRupees(kMaxEntryRupees))
        return Result<Money>::fail(QStringLiteral("That amount is too large."));
    if (note.trimmed().isEmpty())
        return Result<Money>::fail(QStringLiteral("Please add a note explaining the adjustment."));
    if (note.trimmed().size() > 200)
        return Result<Money>::fail(QStringLiteral("The note can be at most 200 characters."));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto customer = loadCustomer(customerId);
    if (!customer)
        return customer.status();
    if (signedAmount.isNegative() && customer.value().balance <= Money())
        return Result<Money>::fail(QStringLiteral("%1 does not owe anything.").arg(customer.value().customerName));
    if (signedAmount.isNegative() && size > customer.value().balance)
        return Result<Money>::fail(QStringLiteral("%1 owes only %2. You cannot reduce the balance below zero.")
                                       .arg(customer.value().customerName, customer.value().balance.toString()));

    auto after = m_repos.khata.append(customerId, KhataKind::Adjustment, signedAmount, 0, note.trimmed(),
                                      session.userId());
    if (!after)
        return after;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("khata_adjustment"), QStringLiteral("user"),
                                    customerId, signedAmount.toPlainString());
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    return after;
}

} // namespace shophub
