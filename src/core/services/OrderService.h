#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

class OrderService
{
public:
    explicit OrderService(Repositories &repos);

    // Customers see only their own orders; admins see everyone's.
    Result<QVector<Order>> myOrders(const Session &session) const;
    Result<QVector<Order>> allOrders(const Session &session, const OrderQuery &query) const;
    Result<Order> details(const Session &session, qint64 orderId) const;
    Result<QVector<OrderStatusChange>> history(const Session &session, qint64 orderId) const;

    // Admin: move an order one step forward (Pending -> Confirmed -> ... -> Delivered).
    // Delivered marks a Cash on Delivery order as paid; khata orders stay "on khata".
    // A qist order can be delivered only after its down payment is recorded.
    Status advance(const Session &session, qint64 orderId, OrderStatus to, const QString &note = QString());

    // Customer (Pending/Confirmed, own orders) or admin (up to Processing).
    // Cancelling puts the items back into stock and takes a khata or qist order off the khata.
    Status cancel(const Session &session, qint64 orderId, const QString &reason);

private:
    Result<Order> loadForViewer(const Session &session, qint64 orderId) const;

    Repositories &m_repos;
};

} // namespace shophub
