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

} // namespace shophub
