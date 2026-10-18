#include "repositories/ProductRepository.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"

#include <QSqlQuery>
#include <QVariantMap>

namespace shophub {

namespace {

// Units sold counts every order except cancelled ones.
const QString kSelectProduct = QStringLiteral(
    "SELECT p.id, p.sku, p.name, p.description, p.category_id, c.name, p.brand, p.price_paisa, "
    "       p.cost_paisa, p.stock, p.low_stock_threshold, p.is_active, p.image_file, "
    "       p.created_at, p.updated_at, COALESCE(s.sold, 0), c.is_active "
    "FROM products p "
    "JOIN categories c ON c.id = p.category_id "
    "LEFT JOIN (SELECT oi.product_id, SUM(oi.quantity) AS sold "
    "           FROM order_items oi JOIN orders o ON o.id = oi.order_id "
    "           WHERE o.status <> 'cancelled' GROUP BY oi.product_id) s ON s.product_id = p.id ");

Product readProduct(const QSqlQuery &q)
{
    Product p;
    p.id = q.value(0).toLongLong();
    p.sku = q.value(1).toString();
    p.name = q.value(2).toString();
    p.description = q.value(3).toString();
    p.categoryId = q.value(4).toLongLong();
    p.categoryName = q.value(5).toString();
    p.brand = q.value(6).toString();
    p.price = Money::fromPaisa(q.value(7).toLongLong());
    if (!q.value(8).isNull())
        p.cost = Money::fromPaisa(q.value(8).toLongLong());
    p.stock = q.value(9).toInt();
    p.lowStockThreshold = q.value(10).toInt();
    p.active = q.value(11).toInt() == 1;
    p.categoryActive = q.value(16).toInt() == 1;
    p.imageFile = q.value(12).toString();
    p.createdAt = sql::fromDb(q.value(13));
    p.updatedAt = sql::fromDb(q.value(14));
    p.unitsSold = q.value(15).toInt();
    return p;
}

QString orderBy(ProductSort sort)
{
    switch (sort) {
    case ProductSort::Newest: return QStringLiteral("ORDER BY p.created_at DESC, p.id DESC");
    case ProductSort::PriceLowToHigh: return QStringLiteral("ORDER BY p.price_paisa ASC, p.id ASC");
    case ProductSort::PriceHighToLow: return QStringLiteral("ORDER BY p.price_paisa DESC, p.id ASC");
    case ProductSort::NameAZ: return QStringLiteral("ORDER BY p.name COLLATE NOCASE ASC, p.id ASC");
    case ProductSort::Popularity:
        return QStringLiteral("ORDER BY COALESCE(s.sold, 0) DESC, p.name COLLATE NOCASE ASC");
    }
    return QStringLiteral("ORDER BY p.id DESC");
}

QVariant costValue(const ProductInput &in)
{
    return in.cost ? QVariant(in.cost->paisa()) : QVariant(QMetaType(QMetaType::LongLong));
}

} // namespace

Result<ProductPage> ProductRepository::search(const ProductFilter &f) const
{
    QStringList where;
    QVariantMap binds;

    if (!f.includeInactive)
        where << QStringLiteral("p.is_active = 1 AND c.is_active = 1");
    if (f.categoryId > 0) {
        where << QStringLiteral("p.category_id = :cat");
        binds[QStringLiteral(":cat")] = f.categoryId;
    }
    if (!f.brand.isEmpty()) {
        where << QStringLiteral("p.brand = :brand");
        binds[QStringLiteral(":brand")] = f.brand;
    }
    if (f.minPrice) {
        where << QStringLiteral("p.price_paisa >= :minp");
        binds[QStringLiteral(":minp")] = f.minPrice->paisa();
    }
    if (f.maxPrice) {
        where << QStringLiteral("p.price_paisa <= :maxp");
        binds[QStringLiteral(":maxp")] = f.maxPrice->paisa();
    }
    if (f.inStockOnly)
        where << QStringLiteral("p.stock > 0");

    const QString text = f.text.trimmed();
    if (!text.isEmpty()) {
        const QString like = QLatin1Char('%') + validation::escapeLike(text) + QLatin1Char('%');
        where << QStringLiteral("(p.name LIKE :t1 ESCAPE '\\' OR p.sku LIKE :t2 ESCAPE '\\' "
                                "OR p.brand LIKE :t3 ESCAPE '\\' OR p.description LIKE :t4 ESCAPE '\\')");
        for (const char *key : {":t1", ":t2", ":t3", ":t4"})
            binds[QString::fromLatin1(key)] = like;
    }

    const QString whereSql = where.isEmpty() ? QString() : QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")) + QLatin1Char(' ');

    auto bindAll = [&binds](QSqlQuery &q) {
        for (auto it = binds.cbegin(); it != binds.cend(); ++it)
            q.bindValue(it.key(), it.value());
    };

    ProductPage page;

    QSqlQuery count(db());
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM products p JOIN categories c ON c.id = p.category_id ") + whereSql);
    bindAll(count);
    if (auto st = sql::exec(count, "ProductRepository::search(count)"); !st)
        return st;
    count.next();
    page.total = count.value(0).toInt();

    QSqlQuery q(db());
    q.prepare(kSelectProduct + whereSql + orderBy(f.sort) + QStringLiteral(" LIMIT :lim OFFSET :off"));
    bindAll(q);
    q.bindValue(QStringLiteral(":lim"), qBound(1, f.limit, 500));
    q.bindValue(QStringLiteral(":off"), qMax(0, f.offset));
    if (auto st = sql::exec(q, "ProductRepository::search"); !st)
        return st;
    while (q.next())
        page.items.push_back(readProduct(q));
    return page;
}

Result<std::optional<Product>> ProductRepository::findById(qint64 id) const
{
    QSqlQuery q(db());
    q.prepare(kSelectProduct + QStringLiteral("WHERE p.id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (auto st = sql::exec(q, "ProductRepository::findById"); !st)
        return st;
    if (!q.next())
        return std::optional<Product>{};
    return std::optional<Product>{readProduct(q)};
}

Result<qint64> ProductRepository::insert(const ProductInput &in, int initialStock)
{
    const QString now = sql::nowUtc();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO products (sku, name, description, category_id, brand, price_paisa, cost_paisa, "
        "  stock, low_stock_threshold, image_file, created_at, updated_at) "
        "VALUES (:sku, :name, :desc, :cat, :brand, :price, :cost, :stock, :low, :img, :c, :u)"));
    q.bindValue(QStringLiteral(":sku"), in.sku);
    q.bindValue(QStringLiteral(":name"), in.name);
    q.bindValue(QStringLiteral(":desc"), sql::text(in.description));
    q.bindValue(QStringLiteral(":cat"), in.categoryId);
    q.bindValue(QStringLiteral(":brand"), sql::text(in.brand));
    q.bindValue(QStringLiteral(":price"), in.price.paisa());
    q.bindValue(QStringLiteral(":cost"), costValue(in));
    q.bindValue(QStringLiteral(":stock"), initialStock);
    q.bindValue(QStringLiteral(":low"), in.lowStockThreshold);
    q.bindValue(QStringLiteral(":img"), sql::text(in.imageFile));
    q.bindValue(QStringLiteral(":c"), now);
    q.bindValue(QStringLiteral(":u"), now);
    if (auto st = sql::exec(q, "ProductRepository::insert"); !st)
        return st;
    return q.lastInsertId().toLongLong();
}

Status ProductRepository::update(qint64 id, const ProductInput &in)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE products SET sku = :sku, name = :name, description = :desc, category_id = :cat, "
        "  brand = :brand, price_paisa = :price, cost_paisa = :cost, low_stock_threshold = :low, "
        "  image_file = :img, updated_at = :u "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":sku"), in.sku);
    q.bindValue(QStringLiteral(":name"), in.name);
    q.bindValue(QStringLiteral(":desc"), sql::text(in.description));
    q.bindValue(QStringLiteral(":cat"), in.categoryId);
    q.bindValue(QStringLiteral(":brand"), sql::text(in.brand));
    q.bindValue(QStringLiteral(":price"), in.price.paisa());
    q.bindValue(QStringLiteral(":cost"), costValue(in));
    q.bindValue(QStringLiteral(":low"), in.lowStockThreshold);
    q.bindValue(QStringLiteral(":img"), sql::text(in.imageFile));
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "ProductRepository::update");
}

Status ProductRepository::setActive(qint64 id, bool active)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE products SET is_active = :a, updated_at = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":a"), active ? 1 : 0);
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "ProductRepository::setActive");
}

Result<bool> ProductRepository::skuExists(const QString &sku, qint64 excludeId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT 1 FROM products WHERE sku = :s AND id <> :id"));
    q.bindValue(QStringLiteral(":s"), sku);
    q.bindValue(QStringLiteral(":id"), excludeId);
    if (auto st = sql::exec(q, "ProductRepository::skuExists"); !st)
        return st;
    return q.next();
}

Result<QStringList> ProductRepository::brands() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT DISTINCT brand FROM products "
                             "WHERE is_active = 1 AND brand <> '' ORDER BY brand COLLATE NOCASE"));
    if (auto st = sql::exec(q, "ProductRepository::brands"); !st)
        return st;
    QStringList out;
    while (q.next())
        out << q.value(0).toString();
    return out;
}

Result<int> ProductRepository::lowStockCount() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM products WHERE is_active = 1 AND stock <= low_stock_threshold"));
    if (auto st = sql::exec(q, "ProductRepository::lowStockCount"); !st)
        return st;
    q.next();
    return q.value(0).toInt();
}

Result<int> ProductRepository::changeStock(qint64 id, int delta)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE products SET stock = stock + :d1, updated_at = :u "
                             "WHERE id = :id AND stock + :d2 >= 0"));
    q.bindValue(QStringLiteral(":d1"), delta);
    q.bindValue(QStringLiteral(":d2"), delta);
    q.bindValue(QStringLiteral(":u"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    if (auto st = sql::exec(q, "ProductRepository::changeStock"); !st)
        return st;
    if (q.numRowsAffected() != 1)
        return Result<int>::fail(QStringLiteral("Not enough stock available."));

    QSqlQuery read(db());
    read.prepare(QStringLiteral("SELECT stock FROM products WHERE id = :id"));
    read.bindValue(QStringLiteral(":id"), id);
    if (auto st = sql::exec(read, "ProductRepository::changeStock(read)"); !st)
        return st;
    read.next();
    return read.value(0).toInt();
}

Status ProductRepository::insertMovement(qint64 productId, int change, int stockAfter, StockReason reason,
                                         const QString &note, qint64 userId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO inventory_movements (product_id, quantity_change, stock_after, reason, note, user_id, created_at) "
        "VALUES (:p, :c, :s, :r, :n, :u, :t)"));
    q.bindValue(QStringLiteral(":p"), productId);
    q.bindValue(QStringLiteral(":c"), change);
    q.bindValue(QStringLiteral(":s"), stockAfter);
    q.bindValue(QStringLiteral(":r"), stockReasonToDb(reason));
    q.bindValue(QStringLiteral(":n"), sql::text(note));
    q.bindValue(QStringLiteral(":u"), sql::nullable(userId));
    q.bindValue(QStringLiteral(":t"), sql::nowUtc());
    return sql::exec(q, "ProductRepository::insertMovement");
}

Result<QVector<InventoryMovement>> ProductRepository::movements(qint64 productId, int limit) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT m.id, m.product_id, p.name, m.quantity_change, m.stock_after, m.reason, m.note, "
        "       COALESCE(u.full_name, ''), m.created_at "
        "FROM inventory_movements m "
        "JOIN products p ON p.id = m.product_id "
        "LEFT JOIN users u ON u.id = m.user_id ")
        + (productId > 0 ? QStringLiteral("WHERE m.product_id = :p ") : QString())
        + QStringLiteral("ORDER BY m.id DESC LIMIT :lim"));
    if (productId > 0)
        q.bindValue(QStringLiteral(":p"), productId);
    q.bindValue(QStringLiteral(":lim"), qBound(1, limit, 1000));
    if (auto st = sql::exec(q, "ProductRepository::movements"); !st)
        return st;

    QVector<InventoryMovement> out;
    while (q.next()) {
        InventoryMovement m;
        m.id = q.value(0).toLongLong();
        m.productId = q.value(1).toLongLong();
        m.productName = q.value(2).toString();
        m.change = q.value(3).toInt();
        m.stockAfter = q.value(4).toInt();
        m.reason = stockReasonFromDb(q.value(5).toString()).value_or(StockReason::Adjustment);
        m.note = q.value(6).toString();
        m.userName = q.value(7).toString();
        m.createdAt = sql::fromDb(q.value(8));
        out.push_back(m);
    }
    return out;
}

} // namespace shophub
