#include "services/OrderService.h"

#include "database/Transaction.h"
#include "services/Access.h"

#include <QtDebug>

namespace shophub {

OrderService::OrderService(Repositories &repos)
    : m_repos(repos)
{
}

Result<QVector<Order>> OrderService::myOrders(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    OrderQuery q;
    q.userId = session.userId();
    return m_repos.orders.list(q);
}

Result<QVector<Order>> OrderService::allOrders(const Session &session, const OrderQuery &query) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return m_repos.orders.list(query);
}

Result<Order> OrderService::loadForViewer(const Session &session, qint64 orderId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.orders.findById(orderId, true);
    if (!found)
        return found.status();
    // Same message for "missing" and "someone else's", so order ids can't be probed.
    if (!found.value() || (!session.isAdmin() && found.value()->userId != session.userId()))
        return Result<Order>::fail(QStringLiteral("Order not found."));
    return *found.value();
}

Result<Order> OrderService::details(const Session &session, qint64 orderId) const
{
    return loadForViewer(session, orderId);
}

Result<QVector<OrderStatusChange>> OrderService::history(const Session &session, qint64 orderId) const
{
    auto order = loadForViewer(session, orderId);
    if (!order)
        return order.status();
    return m_repos.orders.history(orderId);
}

Status OrderService::advance(const Session &session, qint64 orderId, OrderStatus to, const QString &note)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (to == OrderStatus::Cancelled)
        return cancel(session, orderId, note);

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    auto order = loadForViewer(session, orderId);
    if (!order)
        return order.status();
    const OrderStatus from = order.value().status;
    if (!orderstatus::canTransition(from, to))
        return Status::fail(QStringLiteral("An order cannot go from %1 to %2.")
                                .arg(orderstatus::displayName(from), orderstatus::displayName(to)));

    // Cash on Delivery is paid when delivered. A khata order stays "on khata":
    // the customer pays the khata later, not the single order.
    const bool cod = order.value().paymentMethod == payment::kCashOnDelivery;
    const QString paymentStatus = (to == OrderStatus::Delivered && cod) ? payment::kPaid : order.value().paymentStatus;
    if (auto st = m_repos.orders.updateStatus(orderId, to, paymentStatus); !st)
        return st;
    if (auto st = m_repos.orders.addHistory(orderId, from, to, session.userId(), note.trimmed()); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("order_status_changed"), QStringLiteral("order"),
                                    orderId, QStringLiteral("%1 -> %2").arg(orderstatus::toDb(from), orderstatus::toDb(to)));
        !st)
        return st;
    return tx.commit();
}

Status OrderService::cancel(const Session &session, qint64 orderId, const QString &reason)
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    auto order = loadForViewer(session, orderId);
    if (!order)
        return order.status();
    const Order &o = order.value();

    const bool allowed = session.isAdmin() ? orderstatus::adminCanCancel(o.status)
                                           : orderstatus::customerCanCancel(o.status);
    if (!allowed) {
        if (o.status == OrderStatus::Cancelled)
            return Status::fail(QStringLiteral("This order is already cancelled."));
        return Status::fail(session.isAdmin()
                                ? QStringLiteral("Orders that have shipped can no longer be cancelled.")
                                : QStringLiteral("This order is already being processed and can no longer be cancelled. "
                                                 "Please contact support."));
    }

    // Put every line back into stock (skip lines whose product was deleted).
    for (const OrderItem &item : o.items) {
        if (!item.productId)
            continue;
        auto newStock = m_repos.products.changeStock(*item.productId, item.quantity);
        if (!newStock)
            return newStock.status();
        if (auto st = m_repos.products.insertMovement(*item.productId, item.quantity, newStock.value(),
                                                      StockReason::CancelRestock, o.orderNumber, session.userId());
            !st)
            return st;
    }

    // A khata order that is cancelled comes off the customer's khata.
    if (o.paymentMethod == payment::kKhata) {
        auto charged = m_repos.khata.hasEntry(orderId, KhataKind::Charge);
        if (!charged)
            return charged.status();
        auto reversed = m_repos.khata.hasEntry(orderId, KhataKind::Reversal);
        if (!reversed)
            return reversed.status();
        if (charged.value() && !reversed.value()) {
            auto after = m_repos.khata.append(o.userId, KhataKind::Reversal, Money() - o.total, orderId,
                                              o.orderNumber, session.userId());
            if (!after)
                return after.status();
        }
    }

    const QString note = reason.trimmed().isEmpty()
                             ? (session.isAdmin() ? QStringLiteral("Cancelled by store") : QStringLiteral("Cancelled by customer"))
                             : reason.trimmed();
    if (auto st = m_repos.orders.updateStatus(orderId, OrderStatus::Cancelled, o.paymentStatus); !st)
        return st;
    if (auto st = m_repos.orders.addHistory(orderId, o.status, OrderStatus::Cancelled, session.userId(), note); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("order_cancelled"), QStringLiteral("order"),
                                    orderId, o.orderNumber);
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    qInfo().noquote() << "Order cancelled" << o.orderNumber;
    return Status::ok();
}

} // namespace shophub
