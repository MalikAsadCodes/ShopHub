#include "domain/Types.h"

namespace shophub {

QString roleToDb(Role role)
{
    return role == Role::Admin ? QStringLiteral("admin") : QStringLiteral("customer");
}

std::optional<Role> roleFromDb(const QString &s)
{
    if (s == QLatin1String("admin"))
        return Role::Admin;
    if (s == QLatin1String("customer"))
        return Role::Customer;
    return std::nullopt;
}

QString stockReasonToDb(StockReason r)
{
    switch (r) {
    case StockReason::Initial: return QStringLiteral("initial");
    case StockReason::Restock: return QStringLiteral("restock");
    case StockReason::Adjustment: return QStringLiteral("adjustment");
    case StockReason::Damaged: return QStringLiteral("damaged");
    case StockReason::Sale: return QStringLiteral("sale");
    case StockReason::CancelRestock: return QStringLiteral("cancel_restock");
    }
    return QStringLiteral("adjustment");
}

std::optional<StockReason> stockReasonFromDb(const QString &s)
{
    for (StockReason r : {StockReason::Initial, StockReason::Restock, StockReason::Adjustment,
                          StockReason::Damaged, StockReason::Sale, StockReason::CancelRestock}) {
        if (stockReasonToDb(r) == s)
            return r;
    }
    return std::nullopt;
}

QString stockReasonDisplay(StockReason r)
{
    switch (r) {
    case StockReason::Initial: return QStringLiteral("Initial stock");
    case StockReason::Restock: return QStringLiteral("Restock");
    case StockReason::Adjustment: return QStringLiteral("Manual adjustment");
    case StockReason::Damaged: return QStringLiteral("Damaged / lost");
    case StockReason::Sale: return QStringLiteral("Sale");
    case StockReason::CancelRestock: return QStringLiteral("Order cancelled");
    }
    return QString();
}

namespace payment {

QString methodDisplay(const QString &method)
{
    if (method == kKhata)
        return QStringLiteral("Khata");
    return QStringLiteral("Cash on Delivery");
}

QString statusDisplay(const QString &status)
{
    if (status == kPaid)
        return QStringLiteral("Paid");
    if (status == kOnAccount)
        return QStringLiteral("On khata");
    return QStringLiteral("Unpaid");
}

} // namespace payment

QString khataKindToDb(KhataKind k)
{
    switch (k) {
    case KhataKind::Charge: return QStringLiteral("charge");
    case KhataKind::Payment: return QStringLiteral("payment");
    case KhataKind::Reversal: return QStringLiteral("reversal");
    case KhataKind::Adjustment: return QStringLiteral("adjustment");
    }
    return QStringLiteral("adjustment");
}

std::optional<KhataKind> khataKindFromDb(const QString &s)
{
    for (KhataKind k : {KhataKind::Charge, KhataKind::Payment, KhataKind::Reversal, KhataKind::Adjustment}) {
        if (khataKindToDb(k) == s)
            return k;
    }
    return std::nullopt;
}

QString khataKindDisplay(KhataKind k)
{
    switch (k) {
    case KhataKind::Charge: return QStringLiteral("Order on khata");
    case KhataKind::Payment: return QStringLiteral("Payment received");
    case KhataKind::Reversal: return QStringLiteral("Order cancelled");
    case KhataKind::Adjustment: return QStringLiteral("Adjustment");
    }
    return QString();
}

} // namespace shophub
