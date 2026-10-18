#include "domain/OrderStatus.h"

namespace shophub::orderstatus {

QString toDb(OrderStatus s)
{
    switch (s) {
    case OrderStatus::Pending: return QStringLiteral("pending");
    case OrderStatus::Confirmed: return QStringLiteral("confirmed");
    case OrderStatus::Processing: return QStringLiteral("processing");
    case OrderStatus::Shipped: return QStringLiteral("shipped");
    case OrderStatus::Delivered: return QStringLiteral("delivered");
    case OrderStatus::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("pending");
}

std::optional<OrderStatus> fromDb(const QString &s)
{
    for (OrderStatus st : all()) {
        if (toDb(st) == s)
            return st;
    }
    return std::nullopt;
}

QString displayName(OrderStatus s)
{
    QString name = toDb(s);
    name[0] = name[0].toUpper();
    return name;
}

QVector<OrderStatus> all()
{
    return {OrderStatus::Pending, OrderStatus::Confirmed, OrderStatus::Processing,
            OrderStatus::Shipped, OrderStatus::Delivered, OrderStatus::Cancelled};
}

std::optional<OrderStatus> nextForward(OrderStatus from)
{
    switch (from) {
    case OrderStatus::Pending: return OrderStatus::Confirmed;
    case OrderStatus::Confirmed: return OrderStatus::Processing;
    case OrderStatus::Processing: return OrderStatus::Shipped;
    case OrderStatus::Shipped: return OrderStatus::Delivered;
    case OrderStatus::Delivered:
    case OrderStatus::Cancelled: return std::nullopt;
    }
    return std::nullopt;
}

bool adminCanCancel(OrderStatus s)
{
    return s == OrderStatus::Pending || s == OrderStatus::Confirmed || s == OrderStatus::Processing;
}

bool customerCanCancel(OrderStatus s)
{
    return s == OrderStatus::Pending || s == OrderStatus::Confirmed;
}

bool canTransition(OrderStatus from, OrderStatus to)
{
    if (to == OrderStatus::Cancelled)
        return adminCanCancel(from);
    const auto next = nextForward(from);
    return next.has_value() && *next == to;
}

bool isFinal(OrderStatus s)
{
    return s == OrderStatus::Delivered || s == OrderStatus::Cancelled;
}

} // namespace shophub::orderstatus
