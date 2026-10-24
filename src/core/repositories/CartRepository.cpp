#include "repositories/CartRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

Result<QVector<CartItem>> CartRepository::items(qint64 userId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT p.id, p.sku, p.name, p.image_file, c.name, p.price_paisa, ci.quantity, p.stock, "
        "       (p.is_active = 1 AND c.is_active = 1) "
        "FROM carts ca "
        "JOIN cart_items ci ON ci.cart_id = ca.id "
        "JOIN products p ON p.id = ci.product_id "
        "JOIN categories c ON c.id = p.category_id "
        "WHERE ca.user_id = :u "
        "ORDER BY ci.added_at, p.name"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (auto st = sql::exec(q, "CartRepository::items"); !st)
        return st;

    QVector<CartItem> out;
    while (q.next()) {
        CartItem i;
        i.productId = q.value(0).toLongLong();
        i.sku = q.value(1).toString();
        i.name = q.value(2).toString();
        i.imageFile = q.value(3).toString();
        i.categoryName = q.value(4).toString();
        i.basePrice = Money::fromPaisa(q.value(5).toLongLong());
        i.unitPrice = i.basePrice; // wholesale tiers are applied by the services
        i.quantity = q.value(6).toInt();
        i.availableStock = q.value(7).toInt();
        i.productActive = q.value(8).toInt() == 1;
        out.push_back(i);
    }
    return out;
}

Result<int> CartRepository::quantityOf(qint64 userId, qint64 productId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT ci.quantity FROM cart_items ci JOIN carts ca ON ca.id = ci.cart_id "
        "WHERE ca.user_id = :u AND ci.product_id = :p"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":p"), productId);
    if (auto st = sql::exec(q, "CartRepository::quantityOf"); !st)
        return st;
    return q.next() ? q.value(0).toInt() : 0;
}

Result<int> CartRepository::totalQuantity(qint64 userId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(ci.quantity), 0) FROM cart_items ci JOIN carts ca ON ca.id = ci.cart_id "
        "WHERE ca.user_id = :u"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (auto st = sql::exec(q, "CartRepository::totalQuantity"); !st)
        return st;
    q.next();
    return q.value(0).toInt();
}

Result<qint64> CartRepository::ensureCart(qint64 userId)
{
    QSqlQuery insert(db());
    insert.prepare(QStringLiteral(
        "INSERT INTO carts (user_id, updated_at) VALUES (:u, :t) "
        "ON CONFLICT(user_id) DO UPDATE SET updated_at = excluded.updated_at"));
    insert.bindValue(QStringLiteral(":u"), userId);
    insert.bindValue(QStringLiteral(":t"), sql::nowUtc());
    if (auto st = sql::exec(insert, "CartRepository::ensureCart"); !st)
        return st;

    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id FROM carts WHERE user_id = :u"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (auto st = sql::exec(q, "CartRepository::ensureCart(read)"); !st)
        return st;
    if (!q.next())
        return Result<qint64>::fail(sql::kGenericDbError);
    return q.value(0).toLongLong();
}

Status CartRepository::setQuantity(qint64 userId, qint64 productId, int quantity)
{
    auto cart = ensureCart(userId);
    if (!cart)
        return cart.status();

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO cart_items (cart_id, product_id, quantity, added_at) VALUES (:c, :p, :q, :t) "
        "ON CONFLICT(cart_id, product_id) DO UPDATE SET quantity = excluded.quantity"));
    q.bindValue(QStringLiteral(":c"), cart.value());
    q.bindValue(QStringLiteral(":p"), productId);
    q.bindValue(QStringLiteral(":q"), quantity);
    q.bindValue(QStringLiteral(":t"), sql::nowUtc());
    return sql::exec(q, "CartRepository::setQuantity");
}

Status CartRepository::remove(qint64 userId, qint64 productId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "DELETE FROM cart_items WHERE product_id = :p "
        "AND cart_id = (SELECT id FROM carts WHERE user_id = :u)"));
    q.bindValue(QStringLiteral(":p"), productId);
    q.bindValue(QStringLiteral(":u"), userId);
    return sql::exec(q, "CartRepository::remove");
}

Status CartRepository::clear(qint64 userId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "DELETE FROM cart_items WHERE cart_id = (SELECT id FROM carts WHERE user_id = :u)"));
    q.bindValue(QStringLiteral(":u"), userId);
    return sql::exec(q, "CartRepository::clear");
}

} // namespace shophub
