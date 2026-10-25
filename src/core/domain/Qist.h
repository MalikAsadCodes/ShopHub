#pragma once

// Qist (installments) - v2 milestone 3. See docs/QIST.md.
//
// The arithmetic of a qist, kept away from the database so it can be tested on
// its own: a plan's terms, the quote a customer sees at checkout (qist price,
// down payment, monthly installments and their due dates), and how the money
// received so far is spread over that schedule, oldest installment first.

#include "common/Money.h"
#include "common/Result.h"

#include <QDate>
#include <QString>
#include <QVector>
#include <optional>

namespace shophub {

// What a plan promises. Copied onto every qist sold with it, so editing the
// plan later never changes a qist that was already sold.
struct QistTerms
{
    int months = 0;    // monthly installments after the down payment
    int markupBp = 0;  // added to the cash price: 1000 = 10%
    int minDownBp = 0; // smallest down payment, as a share of the qist price: 2000 = 20%
};

// One line of the schedule.
struct QistInstallment
{
    int number = 0; // 0 = down payment, 1..months = monthly installments
    QDate dueDate;
    Money amount;
};

// Everything the customer is shown before placing a qist order.
struct QistQuote
{
    QistTerms terms;
    Money cashPrice;   // the order total
    Money markup;
    Money qistPrice;   // cash price + markup
    Money minimumDown;
    Money downPayment;
    Money monthly;     // installments 1 .. months-1, whole rupees
    Money lastMonthly; // the final installment, which takes the odd rupees and paisa
    QVector<QistInstallment> schedule;
};

enum class InstallmentState { Paid, Overdue, DueToday, Upcoming };

// One schedule line with what has been paid towards it so far.
struct InstallmentStatus
{
    QistInstallment installment;
    Money paid;
    InstallmentState state = InstallmentState::Upcoming;

    Money remaining() const { return installment.amount - paid; }
};

namespace qist {

constexpr int kMinMonths = 2;
constexpr int kMaxMonths = 24;
constexpr int kMaxMarkupBp = 5000;        // 50%
constexpr int kMaxDownBp = 9000;          // 90%
constexpr qint64 kMinMonthlyRupees = 100; // smallest monthly installment

// Empty when the terms are fine, otherwise a message for the admin.
inline QString validateTerms(const QistTerms &t)
{
    if (t.months < kMinMonths || t.months > kMaxMonths)
        return QStringLiteral("A plan must run for %1 to %2 months.").arg(kMinMonths).arg(kMaxMonths);
    if (t.markupBp < 0 || t.markupBp > kMaxMarkupBp)
        return QStringLiteral("The markup must be between 0% and %1%.").arg(kMaxMarkupBp / 100);
    if (t.minDownBp < 0 || t.minDownBp > kMaxDownBp)
        return QStringLiteral("The minimum down payment must be between 0% and %1%.").arg(kMaxDownBp / 100);
    return QString();
}

// The markup on a cash price, rounded half-up to whole paisa (like tax).
inline Money markupFor(Money cashPrice, int markupBp)
{
    return cashPrice.percentOf(markupBp);
}

// The smallest down payment for a qist price, rounded UP to whole rupees so it
// is never below the plan's percentage. Never more than the qist price.
inline Money minimumDown(Money qistPrice, int minDownBp)
{
    const qint64 exact = qistPrice.percentOf(minDownBp).paisa();
    const qint64 wholeRupees = (exact + 99) / 100;
    return Money::fromPaisa(qMin(wholeRupees * 100, qistPrice.paisa()));
}

// Works out a qist for an order total. downPayment empty = the minimum.
// start = the order date: the down payment is due then, installment k is due
// k months later on the same day (31 Jan -> 28/29 Feb -> 31 Mar ...).
inline Result<QistQuote> quote(const QistTerms &terms, Money cashPrice, std::optional<Money> downPayment,
                               const QDate &start)
{
    if (const QString error = validateTerms(terms); !error.isEmpty())
        return Result<QistQuote>::fail(error);
    if (cashPrice <= Money())
        return Result<QistQuote>::fail(QStringLiteral("There is nothing to pay in installments."));
    if (!start.isValid())
        return Result<QistQuote>::fail(QStringLiteral("The qist needs a start date."));

    QistQuote q;
    q.terms = terms;
    q.cashPrice = cashPrice;
    q.markup = markupFor(cashPrice, terms.markupBp);
    q.qistPrice = cashPrice + q.markup;
    q.minimumDown = minimumDown(q.qistPrice, terms.minDownBp);
    q.downPayment = downPayment ? *downPayment : q.minimumDown;

    if (q.downPayment.isNegative())
        return Result<QistQuote>::fail(QStringLiteral("The down payment cannot be negative."));
    if (q.downPayment < q.minimumDown)
        return Result<QistQuote>::fail(
            QStringLiteral("The down payment must be at least %1.").arg(q.minimumDown.toString()));
    if (q.downPayment >= q.qistPrice)
        return Result<QistQuote>::fail(
            QStringLiteral("The down payment must be less than the qist price (%1). To pay everything now, "
                           "choose Cash on Delivery.")
                .arg(q.qistPrice.toString()));

    const Money rest = q.qistPrice - q.downPayment;
    const qint64 monthlyRupees = rest.paisa() / terms.months / 100;
    if (monthlyRupees < kMinMonthlyRupees)
        return Result<QistQuote>::fail(
            QStringLiteral("Each monthly installment must be at least %1. Choose a plan with fewer months "
                           "or a smaller down payment.")
                .arg(Money::fromRupees(kMinMonthlyRupees).toString()));

    q.monthly = Money::fromRupees(monthlyRupees);
    q.lastMonthly = rest - q.monthly * (terms.months - 1);
    if (q.downPayment > Money())
        q.schedule.push_back({0, start, q.downPayment});
    for (int k = 1; k <= terms.months; ++k)
        q.schedule.push_back({k, start.addMonths(k), k < terms.months ? q.monthly : q.lastMonthly});
    return q;
}

// Spreads the money received so far over the schedule (sorted by number),
// oldest installment first, and says where each line stands on `today`.
inline QVector<InstallmentStatus> applyPayments(const QVector<QistInstallment> &schedule, Money paid,
                                                const QDate &today)
{
    QVector<InstallmentStatus> out;
    out.reserve(schedule.size());
    Money left = paid;
    for (const QistInstallment &line : schedule) {
        InstallmentStatus s;
        s.installment = line;
        if (left >= line.amount)
            s.paid = line.amount;
        else if (left > Money())
            s.paid = left;
        left -= s.paid;

        if (s.paid == line.amount)
            s.state = InstallmentState::Paid;
        else if (line.dueDate < today)
            s.state = InstallmentState::Overdue;
        else if (line.dueDate == today)
            s.state = InstallmentState::DueToday;
        else
            s.state = InstallmentState::Upcoming;
        out.push_back(s);
    }
    return out;
}

// The first line that is not fully paid, if any.
inline std::optional<InstallmentStatus> nextUnpaid(const QVector<InstallmentStatus> &lines)
{
    for (const InstallmentStatus &s : lines) {
        if (s.state != InstallmentState::Paid)
            return s;
    }
    return std::nullopt;
}

// What is owed on lines whose due date has passed.
inline Money overdueAmount(const QVector<InstallmentStatus> &lines)
{
    Money sum;
    for (const InstallmentStatus &s : lines) {
        if (s.state == InstallmentState::Overdue)
            sum += s.remaining();
    }
    return sum;
}

// Days since the oldest unpaid due date passed (0 when nothing is overdue).
inline int daysOverdue(const QVector<InstallmentStatus> &lines, const QDate &today)
{
    for (const InstallmentStatus &s : lines) {
        if (s.state == InstallmentState::Overdue)
            return int(s.installment.dueDate.daysTo(today));
    }
    return 0;
}

// "Down payment", "Installment 2 of 6" ...
inline QString installmentName(int number, int months)
{
    return number == 0 ? QStringLiteral("Down payment")
                       : QStringLiteral("Installment %1 of %2").arg(number).arg(months);
}

// What a payment of `amount` covers, given `paidBefore` was already received:
// "Down payment", "Installment 2 of 6", "Part of installment 3 of 6",
// "Installment 2 of 6 to installment 3 of 6". Used as the ledger note.
inline QString describePayment(const QVector<QistInstallment> &schedule, Money paidBefore, Money amount)
{
    if (schedule.isEmpty() || amount <= Money())
        return QString();
    const int months = schedule.last().number;
    const Money after = paidBefore + amount;
    Money cumulative;
    int first = -1;
    int last = -1;
    bool exact = true;
    for (int i = 0; i < schedule.size(); ++i) {
        cumulative += schedule[i].amount;
        if (first < 0 && cumulative > paidBefore)
            first = i;
        if (last < 0 && cumulative >= after) {
            last = i;
            exact = cumulative == after;
        }
    }
    if (first < 0)
        return QString();
    if (last < 0)
        last = int(schedule.size()) - 1;

    auto name = [&](int index) { return installmentName(schedule[index].number, months); };
    if (first == last)
        return exact ? name(first) : QStringLiteral("Part of ") + name(first).toLower();
    return name(first) + QStringLiteral(" to ") + name(last).toLower() + (exact ? QString() : QStringLiteral(" (part)"));
}

inline QString stateText(InstallmentState state)
{
    switch (state) {
    case InstallmentState::Paid: return QStringLiteral("Paid");
    case InstallmentState::Overdue: return QStringLiteral("Overdue");
    case InstallmentState::DueToday: return QStringLiteral("Due today");
    case InstallmentState::Upcoming: return QStringLiteral("Upcoming");
    }
    return QString();
}

} // namespace qist
} // namespace shophub
