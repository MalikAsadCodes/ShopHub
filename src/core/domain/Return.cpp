#include "domain/Return.h"

namespace shophub::returns {

QString statusToDb(ReturnStatus s)
{
    switch (s) {
    case ReturnStatus::Requested: return QStringLiteral("requested");
    case ReturnStatus::Approved: return QStringLiteral("approved");
    case ReturnStatus::Rejected: return QStringLiteral("rejected");
    case ReturnStatus::Cancelled: return QStringLiteral("cancelled");
    case ReturnStatus::Received: return QStringLiteral("received");
    case ReturnStatus::Completed: return QStringLiteral("completed");
    }
    return QStringLiteral("requested");
}

std::optional<ReturnStatus> statusFromDb(const QString &s)
{
    for (ReturnStatus v : {ReturnStatus::Requested, ReturnStatus::Approved, ReturnStatus::Rejected,
                           ReturnStatus::Cancelled, ReturnStatus::Received, ReturnStatus::Completed}) {
        if (statusToDb(v) == s)
            return v;
    }
    return std::nullopt;
}

QString statusDisplay(ReturnStatus s)
{
    switch (s) {
    case ReturnStatus::Requested: return QStringLiteral("Waiting for review");
    case ReturnStatus::Approved: return QStringLiteral("Approved");
    case ReturnStatus::Rejected: return QStringLiteral("Rejected");
    case ReturnStatus::Cancelled: return QStringLiteral("Cancelled");
    case ReturnStatus::Received: return QStringLiteral("Received");
    case ReturnStatus::Completed: return QStringLiteral("Completed");
    }
    return QString();
}

QString statusBadgeKind(ReturnStatus s)
{
    switch (s) {
    case ReturnStatus::Requested: return QStringLiteral("warning");
    case ReturnStatus::Approved: return QStringLiteral("info");
    case ReturnStatus::Received: return QStringLiteral("accent");
    case ReturnStatus::Completed: return QStringLiteral("success");
    case ReturnStatus::Rejected: return QStringLiteral("danger");
    case ReturnStatus::Cancelled: return QStringLiteral("neutral");
    }
    return QStringLiteral("neutral");
}

QString reasonToDb(ReturnReason r)
{
    switch (r) {
    case ReturnReason::WrongItem: return QStringLiteral("wrong_item");
    case ReturnReason::Damaged: return QStringLiteral("damaged");
    case ReturnReason::Defective: return QStringLiteral("defective");
    case ReturnReason::NotAsDescribed: return QStringLiteral("not_as_described");
    case ReturnReason::ChangedMind: return QStringLiteral("changed_mind");
    }
    return QStringLiteral("defective");
}

std::optional<ReturnReason> reasonFromDb(const QString &s)
{
    for (ReturnReason r : allReasons()) {
        if (reasonToDb(r) == s)
            return r;
    }
    return std::nullopt;
}

QString reasonDisplay(ReturnReason r)
{
    switch (r) {
    case ReturnReason::WrongItem: return QStringLiteral("Wrong item sent");
    case ReturnReason::Damaged: return QStringLiteral("Arrived damaged");
    case ReturnReason::Defective: return QStringLiteral("Doesn't work properly");
    case ReturnReason::NotAsDescribed: return QStringLiteral("Not as described");
    case ReturnReason::ChangedMind: return QStringLiteral("Changed my mind");
    }
    return QString();
}

QVector<ReturnReason> allReasons()
{
    return {ReturnReason::WrongItem, ReturnReason::Damaged, ReturnReason::Defective, ReturnReason::NotAsDescribed,
            ReturnReason::ChangedMind};
}

bool isShopFault(ReturnReason r)
{
    return r != ReturnReason::ChangedMind;
}

bool isOpen(ReturnStatus s)
{
    return s == ReturnStatus::Requested || s == ReturnStatus::Approved || s == ReturnStatus::Received;
}

bool canTransition(ReturnStatus from, ReturnStatus to)
{
    switch (from) {
    case ReturnStatus::Requested:
        return to == ReturnStatus::Approved || to == ReturnStatus::Rejected || to == ReturnStatus::Cancelled;
    case ReturnStatus::Approved:
        return to == ReturnStatus::Received || to == ReturnStatus::Cancelled;
    case ReturnStatus::Received:
        return to == ReturnStatus::Completed;
    case ReturnStatus::Rejected:
    case ReturnStatus::Cancelled:
    case ReturnStatus::Completed:
        return false;
    }
    return false;
}

QString numberFor(qint64 returnId)
{
    return QStringLiteral("RET-%1").arg(returnId, 6, 10, QLatin1Char('0'));
}

QDate lastDay(const QDateTime &deliveredUtc, int windowDays)
{
    if (!deliveredUtc.isValid())
        return QDate();
    return deliveredUtc.toLocalTime().date().addDays(qBound(0, windowDays, kMaxWindowDays));
}

QString refundMethodToDb(RefundMethod m)
{
    switch (m) {
    case RefundMethod::Cash: return QStringLiteral("cash");
    case RefundMethod::Easypaisa: return QStringLiteral("easypaisa");
    case RefundMethod::JazzCash: return QStringLiteral("jazzcash");
    case RefundMethod::Bank: return QStringLiteral("bank");
    case RefundMethod::Khata: return QStringLiteral("khata");
    case RefundMethod::Qist: return QStringLiteral("qist");
    }
    return QStringLiteral("cash");
}

std::optional<RefundMethod> refundMethodFromDb(const QString &s)
{
    for (RefundMethod m : {RefundMethod::Cash, RefundMethod::Easypaisa, RefundMethod::JazzCash, RefundMethod::Bank,
                           RefundMethod::Khata, RefundMethod::Qist}) {
        if (refundMethodToDb(m) == s)
            return m;
    }
    return std::nullopt;
}

QString refundMethodDisplay(RefundMethod m)
{
    switch (m) {
    case RefundMethod::Cash: return QStringLiteral("Cash");
    case RefundMethod::Easypaisa: return QStringLiteral("Easypaisa");
    case RefundMethod::JazzCash: return QStringLiteral("JazzCash");
    case RefundMethod::Bank: return QStringLiteral("Bank transfer");
    case RefundMethod::Khata: return QStringLiteral("Khata credit");
    case RefundMethod::Qist: return QStringLiteral("Off the qist");
    }
    return QString();
}

bool paysBack(RefundMethod m)
{
    return m == RefundMethod::Cash || m == RefundMethod::Easypaisa || m == RefundMethod::JazzCash
           || m == RefundMethod::Bank;
}

bool needsReference(RefundMethod m)
{
    return m == RefundMethod::Easypaisa || m == RefundMethod::JazzCash || m == RefundMethod::Bank;
}

Money taxForUnits(Money lineTax, int bought, int units, int creditedUnits, Money creditedTax)
{
    if (bought <= 0 || units <= 0)
        return Money();
    const Money left = lineTax > creditedTax ? lineTax - creditedTax : Money();
    if (creditedUnits + units >= bought)
        return left; // the last units of the line take what is left
    // lineTax * units / bought, half-up, in whole paisa
    const qint64 share = (lineTax.paisa() * units * 2 + bought) / (2 * qint64(bought));
    const Money tax = Money::fromPaisa(share);
    return tax < left ? tax : left;
}

} // namespace shophub::returns
