#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

namespace shophub {

struct OrderQuery
{
    qint64 userId = 0;                  // 0 = all customers
    std::optional<OrderStatus> status;  // empty = all statuses
    QString search;                     // order number, customer name or email
    int limit = 500;
};

class OrderRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    // Inserts the header and all lines. Returns the new order id.
    Result<qint64> insert(const Order &order);
    Result<std::optional<qint64>> findIdByToken(const QString &checkoutToken) const;
    Result<std::optional<Order>> findById(qint64 id, bool withItems) const;
    Result<QVector<Order>> list(const OrderQuery &query) const;

    Status updateStatus(qint64 id, OrderStatus status, const QString &paymentStatus);
    Status setPaymentStatus(qint64 id, const QString &paymentStatus);
    Status addHistory(qint64 orderId, std::optional<OrderStatus> from, OrderStatus to,
                      qint64 changedBy, const QString &note);
    Result<QVector<OrderStatusChange>> history(qint64 orderId) const;

    // ---- reporting (cancelled orders are excluded everywhere)
    Result<QPair<Money, int>> salesTotals() const;            // gross sales, order count
    Result<int> countWithStatus(const QVector<OrderStatus> &statuses) const;
    Result<QVector<TopProduct>> topProducts(int limit) const;
    Result<QVector<QPair<QDateTime, Money>>> salesSince(const QDateTime &since) const;
};

} // namespace shophub
