#include "repositories/OrderRepository.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

namespace {

const QString kSelectOrder = QStringLiteral(
    "SELECT o.id, o.order_number, o.user_id, u.full_name, u.email, o.status, o.subtotal_paisa, "
    "       o.shipping_paisa, o.tax_paisa, o.total_paisa, o.payment_method, o.payment_status, "
    "       o.ship_name, o.ship_phone, o.ship_line1, o.ship_city, o.checkout_token, "
    "       o.created_at, o.updated_at, "
    "       (SELECT COALESCE(SUM(quantity), 0) FROM order_items WHERE order_id = o.id) "
    "FROM orders o JOIN users u ON u.id = o.user_id ");

Order readOrder(const QSqlQuery &q)
{
    Order o;
    o.id = q.value(0).toLongLong();
    o.orderNumber = q.value(1).toString();
    o.userId = q.value(2).toLongLong();
    o.customerName = q.value(3).toString();
    o.customerEmail = q.value(4).toString();
    o.status = orderstatus::fromDb(q.value(5).toString()).value_or(OrderStatus::Pending);
    o.subtotal = Money::fromPaisa(q.value(6).toLongLong());
    o.shipping = Money::fromPaisa(q.value(7).toLongLong());
    o.tax = Money::fromPaisa(q.value(8).toLongLong());
    o.total = Money::fromPaisa(q.value(9).toLongLong());
    o.paymentMethod = q.value(10).toString();
    o.paymentStatus = q.value(11).toString();
    o.shipName = q.value(12).toString();
    o.shipPhone = q.value(13).toString();
    o.shipLine1 = q.value(14).toString();
    o.shipCity = q.value(15).toString();
    o.checkoutToken = q.value(16).toString();
    o.createdAt = sql::fromDb(q.value(17));
    o.updatedAt = sql::fromDb(q.value(18));
    o.itemCount = q.value(19).toInt();
    return o;
}

QString statusList(const QVector<OrderStatus> &statuses)
{
    QStringList parts;
    for (OrderStatus s : statuses)
        parts << QStringLiteral("'%1'").arg(orderstatus::toDb(s)); // fixed enum values, not user input
    return parts.join(QLatin1Char(','));
}

} // namespace

Result<qint64> OrderRepository::insert(const Order &o)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO orders (order_number, user_id, status, subtotal_paisa, shipping_paisa, tax_paisa, "
        "  total_paisa, payment_method, payment_status, ship_name, ship_phone, ship_line1, ship_city, "
        "  checkout_token, created_at, updated_at) "
        "VALUES (:num, :user, :status, :sub, :ship, :tax, :total, :pm, :ps, :sn, :sp, :sl, :sc, :tok, :c, :u)"));
    const QString now = sql::nowUtc();
    q.bindValue(QStringLiteral(":num"), o.orderNumber);
    q.bindValue(QStringLiteral(":user"), o.userId);
    q.bindValue(QStringLiteral(":status"), orderstatus::toDb(o.status));
    q.bindValue(QStringLiteral(":sub"), o.subtotal.paisa());
    q.bindValue(QStringLiteral(":ship"), o.shipping.paisa());
    q.bindValue(QStringLiteral(":tax"), o.tax.paisa());
    q.bindValue(QStringLiteral(":total"), o.total.paisa());
    q.bindValue(QStringLiteral(":pm"), o.paymentMethod);
    q.bindValue(QStringLiteral(":ps"), o.paymentStatus);
    q.bindValue(QStringLiteral(":sn"), o.shipName);
    q.bindValue(QStringLiteral(":sp"), o.shipPhone);
    q.bindValue(QStringLiteral(":sl"), o.shipLine1);
    q.bindValue(QStringLiteral(":sc"), o.shipCity);
    q.bindValue(QStringLiteral(":tok"), o.checkoutToken);
    q.bindValue(QStringLiteral(":c"), now);
    q.bindValue(QStringLiteral(":u"), now);
    if (auto st = sql::exec(q, "OrderRepository::insert"); !st)
        return st;
    const qint64 orderId = q.lastInsertId().toLongLong();

    for (const OrderItem &item : o.items) {
        QSqlQuery li(db());
        li.prepare(QStringLiteral(
            "INSERT INTO order_items (order_id, product_id, sku, product_name, unit_price_paisa, "
            "  quantity, line_total_paisa) VALUES (:o, :p, :sku, :name, :price, :qty, :line)"));
        li.bindValue(QStringLiteral(":o"), orderId);
        li.bindValue(QStringLiteral(":p"), sql::nullable(item.productId.value_or(0)));
        li.bindValue(QStringLiteral(":sku"), item.sku);
        li.bindValue(QStringLiteral(":name"), item.productName);
        li.bindValue(QStringLiteral(":price"), item.unitPrice.paisa());
        li.bindValue(QStringLiteral(":qty"), item.quantity);
        li.bindValue(QStringLiteral(":line"), item.lineTotal.paisa());
        if (auto st = sql::exec(li, "OrderRepository::insert(item)"); !st)
            return st;
    }
    return orderId;
}

Result<std::optional<qint64>> OrderRepository::findIdByToken(const QString &checkoutToken) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id FROM orders WHERE checkout_token = :t"));
    q.bindValue(QStringLiteral(":t"), checkoutToken);
    if (auto st = sql::exec(q, "OrderRepository::findIdByToken"); !st)
        return st;
    if (!q.next())
        return std::optional<qint64>{};
    return std::optional<qint64>{q.value(0).toLongLong()};
}

Result<std::optional<Order>> OrderRepository::findById(qint64 id, bool withItems) const
{
    QSqlQuery q(db());
    q.prepare(kSelectOrder + QStringLiteral("WHERE o.id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (auto st = sql::exec(q, "OrderRepository::findById"); !st)
        return st;
    if (!q.next())
        return std::optional<Order>{};
    Order order = readOrder(q);

    if (withItems) {
        QSqlQuery li(db());
        li.prepare(QStringLiteral(
            "SELECT id, product_id, sku, product_name, unit_price_paisa, quantity, line_total_paisa "
            "FROM order_items WHERE order_id = :o ORDER BY id"));
        li.bindValue(QStringLiteral(":o"), id);
        if (auto st = sql::exec(li, "OrderRepository::findById(items)"); !st)
            return st;
        while (li.next()) {
            OrderItem item;
            item.id = li.value(0).toLongLong();
            if (!li.value(1).isNull())
                item.productId = li.value(1).toLongLong();
            item.sku = li.value(2).toString();
            item.productName = li.value(3).toString();
            item.unitPrice = Money::fromPaisa(li.value(4).toLongLong());
            item.quantity = li.value(5).toInt();
            item.lineTotal = Money::fromPaisa(li.value(6).toLongLong());
            order.items.push_back(item);
        }
    }
    return std::optional<Order>{order};
}

Result<QVector<Order>> OrderRepository::list(const OrderQuery &query) const
{
    QStringList where;
    if (query.userId > 0)
        where << QStringLiteral("o.user_id = :user");
    if (query.status)
        where << QStringLiteral("o.status = :status");
    const QString text = query.search.trimmed();
    if (!text.isEmpty())
        where << QStringLiteral("(o.order_number LIKE :s1 ESCAPE '\\' OR u.full_name LIKE :s2 ESCAPE '\\' "
                                "OR u.email LIKE :s3 ESCAPE '\\')");

    QSqlQuery q(db());
    q.prepare(kSelectOrder
              + (where.isEmpty() ? QString() : QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")))
              + QStringLiteral(" ORDER BY o.created_at DESC, o.id DESC LIMIT :lim"));
    if (query.userId > 0)
        q.bindValue(QStringLiteral(":user"), query.userId);
    if (query.status)
        q.bindValue(QStringLiteral(":status"), orderstatus::toDb(*query.status));
    if (!text.isEmpty()) {
        const QString like = QLatin1Char('%') + validation::escapeLike(text) + QLatin1Char('%');
        q.bindValue(QStringLiteral(":s1"), like);
        q.bindValue(QStringLiteral(":s2"), like);
        q.bindValue(QStringLiteral(":s3"), like);
    }
    q.bindValue(QStringLiteral(":lim"), qBound(1, query.limit, 5000));
    if (auto st = sql::exec(q, "OrderRepository::list"); !st)
        return st;

    QVector<Order> out;
    while (q.next())
        out.push_back(readOrder(q));
    return out;
}

Status OrderRepository::updateStatus(qint64 id, OrderStatus status, const QString &paymentStatus)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE orders SET status = :s, payment_status = :p, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":s"), orderstatus::toDb(status));
    q.bindValue(QStringLiteral(":p"), paymentStatus);
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "OrderRepository::updateStatus");
}

Status OrderRepository::addHistory(qint64 orderId, std::optional<OrderStatus> from, OrderStatus to,
                                   qint64 changedBy, const QString &note)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO order_status_history (order_id, from_status, to_status, changed_by, note, changed_at) "
        "VALUES (:o, :f, :t, :by, :n, :at)"));
    q.bindValue(QStringLiteral(":o"), orderId);
    q.bindValue(QStringLiteral(":f"), from ? QVariant(orderstatus::toDb(*from)) : QVariant(QMetaType(QMetaType::QString)));
    q.bindValue(QStringLiteral(":t"), orderstatus::toDb(to));
    q.bindValue(QStringLiteral(":by"), sql::nullable(changedBy));
    q.bindValue(QStringLiteral(":n"), sql::text(note));
    q.bindValue(QStringLiteral(":at"), sql::nowUtc());
    return sql::exec(q, "OrderRepository::addHistory");
}

Result<QVector<OrderStatusChange>> OrderRepository::history(qint64 orderId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT h.from_status, h.to_status, COALESCE(u.full_name, ''), h.note, h.changed_at "
        "FROM order_status_history h LEFT JOIN users u ON u.id = h.changed_by "
        "WHERE h.order_id = :o ORDER BY h.id"));
    q.bindValue(QStringLiteral(":o"), orderId);
    if (auto st = sql::exec(q, "OrderRepository::history"); !st)
        return st;

    QVector<OrderStatusChange> out;
    while (q.next()) {
        OrderStatusChange c;
        if (!q.value(0).isNull())
            c.from = orderstatus::fromDb(q.value(0).toString());
        c.to = orderstatus::fromDb(q.value(1).toString()).value_or(OrderStatus::Pending);
        c.changedBy = q.value(2).toString();
        c.note = q.value(3).toString();
        c.changedAt = sql::fromDb(q.value(4));
        out.push_back(c);
    }
    return out;
}

Result<QPair<Money, int>> OrderRepository::salesTotals() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(total_paisa), 0), COUNT(*) FROM orders WHERE status <> 'cancelled'"));
    if (auto st = sql::exec(q, "OrderRepository::salesTotals"); !st)
        return st;
    q.next();
    return qMakePair(Money::fromPaisa(q.value(0).toLongLong()), q.value(1).toInt());
}

Result<int> OrderRepository::countWithStatus(const QVector<OrderStatus> &statuses) const
{
    if (statuses.isEmpty())
        return 0;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM orders WHERE status IN (%1)").arg(statusList(statuses)));
    if (auto st = sql::exec(q, "OrderRepository::countWithStatus"); !st)
        return st;
    q.next();
    return q.value(0).toInt();
}

Result<QVector<TopProduct>> OrderRepository::topProducts(int limit) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT oi.product_name, SUM(oi.quantity) AS units, SUM(oi.line_total_paisa) "
        "FROM order_items oi JOIN orders o ON o.id = oi.order_id "
        "WHERE o.status <> 'cancelled' "
        "GROUP BY COALESCE(oi.product_id, oi.sku) "
        "ORDER BY units DESC, oi.product_name LIMIT :lim"));
    q.bindValue(QStringLiteral(":lim"), qBound(1, limit, 100));
    if (auto st = sql::exec(q, "OrderRepository::topProducts"); !st)
        return st;
    QVector<TopProduct> out;
    while (q.next())
        out.push_back({q.value(0).toString(), q.value(1).toInt(), Money::fromPaisa(q.value(2).toLongLong())});
    return out;
}

Result<QVector<QPair<QDateTime, Money>>> OrderRepository::salesSince(const QDateTime &since) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT created_at, total_paisa FROM orders WHERE status <> 'cancelled' AND created_at >= :s"));
    q.bindValue(QStringLiteral(":s"), sql::toDb(since));
    if (auto st = sql::exec(q, "OrderRepository::salesSince"); !st)
        return st;
    QVector<QPair<QDateTime, Money>> out;
    while (q.next())
        out.push_back(qMakePair(sql::fromDb(q.value(0)), Money::fromPaisa(q.value(1).toLongLong())));
    return out;
}

} // namespace shophub
