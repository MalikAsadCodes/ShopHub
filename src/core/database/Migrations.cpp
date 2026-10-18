#include "database/Migrations.h"

#include "database/SqlUtil.h"
#include "database/Transaction.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtDebug>

namespace shophub {

namespace {

// --------------------------------------------------------------------------
// Migration 1: initial ShopHub v1 schema.
// Money columns are INTEGER paisa. Timestamps are ISO-8601 UTC text.
// --------------------------------------------------------------------------
QStringList migration1()
{
    return {
        QStringLiteral(R"(
CREATE TABLE users (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    email           TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    full_name       TEXT    NOT NULL CHECK (length(full_name) BETWEEN 1 AND 100),
    password_hash   TEXT    NOT NULL,
    role            TEXT    NOT NULL CHECK (role IN ('customer', 'admin')),
    is_active       INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    failed_logins   INTEGER NOT NULL DEFAULT 0 CHECK (failed_logins >= 0),
    locked_until    TEXT,
    created_at      TEXT    NOT NULL,
    updated_at      TEXT    NOT NULL
))"),
        QStringLiteral(R"(
CREATE TABLE addresses (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    full_name   TEXT    NOT NULL,
    phone       TEXT    NOT NULL,
    line1       TEXT    NOT NULL,
    city        TEXT    NOT NULL,
    is_default  INTEGER NOT NULL DEFAULT 1 CHECK (is_default IN (0, 1)),
    created_at  TEXT    NOT NULL,
    updated_at  TEXT    NOT NULL
))"),
        QStringLiteral("CREATE INDEX idx_addresses_user ON addresses(user_id)"),
        QStringLiteral(R"(
CREATE TABLE categories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL UNIQUE COLLATE NOCASE CHECK (length(name) BETWEEN 1 AND 60),
    parent_id   INTEGER REFERENCES categories(id) ON DELETE RESTRICT,
    is_active   INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    created_at  TEXT    NOT NULL
))"),
        QStringLiteral(R"(
CREATE TABLE products (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    sku                 TEXT    NOT NULL UNIQUE,
    name                TEXT    NOT NULL CHECK (length(name) BETWEEN 1 AND 120),
    description         TEXT    NOT NULL DEFAULT '',
    category_id         INTEGER NOT NULL REFERENCES categories(id) ON DELETE RESTRICT,
    brand               TEXT    NOT NULL DEFAULT '',
    price_paisa         INTEGER NOT NULL CHECK (price_paisa > 0),
    cost_paisa          INTEGER CHECK (cost_paisa IS NULL OR cost_paisa >= 0),
    stock               INTEGER NOT NULL DEFAULT 0 CHECK (stock >= 0),
    low_stock_threshold INTEGER NOT NULL DEFAULT 5 CHECK (low_stock_threshold >= 0),
    is_active           INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    image_file          TEXT    NOT NULL DEFAULT '',
    created_at          TEXT    NOT NULL,
    updated_at          TEXT    NOT NULL
))"),
        QStringLiteral("CREATE INDEX idx_products_category ON products(category_id)"),
        QStringLiteral("CREATE INDEX idx_products_name ON products(name COLLATE NOCASE)"),
        QStringLiteral("CREATE INDEX idx_products_price ON products(price_paisa)"),
        QStringLiteral("CREATE INDEX idx_products_brand ON products(brand)"),
        QStringLiteral("CREATE INDEX idx_products_created ON products(created_at)"),
        QStringLiteral(R"(
CREATE TABLE carts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    updated_at  TEXT    NOT NULL
))"),
        QStringLiteral(R"(
CREATE TABLE cart_items (
    cart_id     INTEGER NOT NULL REFERENCES carts(id) ON DELETE CASCADE,
    product_id  INTEGER NOT NULL REFERENCES products(id) ON DELETE CASCADE,
    quantity    INTEGER NOT NULL CHECK (quantity BETWEEN 1 AND 99),
    added_at    TEXT    NOT NULL,
    PRIMARY KEY (cart_id, product_id)
))"),
        QStringLiteral(R"(
CREATE TABLE orders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    order_number    TEXT    NOT NULL UNIQUE,
    user_id         INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    status          TEXT    NOT NULL CHECK (status IN ('pending', 'confirmed', 'processing',
                                                       'shipped', 'delivered', 'cancelled')),
    subtotal_paisa  INTEGER NOT NULL CHECK (subtotal_paisa >= 0),
    shipping_paisa  INTEGER NOT NULL CHECK (shipping_paisa >= 0),
    tax_paisa       INTEGER NOT NULL CHECK (tax_paisa >= 0),
    total_paisa     INTEGER NOT NULL CHECK (total_paisa = subtotal_paisa + shipping_paisa + tax_paisa),
    payment_method  TEXT    NOT NULL CHECK (payment_method IN ('cod')),
    payment_status  TEXT    NOT NULL CHECK (payment_status IN ('unpaid', 'paid')),
    ship_name       TEXT    NOT NULL,
    ship_phone      TEXT    NOT NULL,
    ship_line1      TEXT    NOT NULL,
    ship_city       TEXT    NOT NULL,
    checkout_token  TEXT    NOT NULL UNIQUE,
    created_at      TEXT    NOT NULL,
    updated_at      TEXT    NOT NULL
))"),
        QStringLiteral("CREATE INDEX idx_orders_user ON orders(user_id)"),
        QStringLiteral("CREATE INDEX idx_orders_status ON orders(status)"),
        QStringLiteral("CREATE INDEX idx_orders_created ON orders(created_at)"),
        QStringLiteral(R"(
CREATE TABLE order_items (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    order_id            INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE,
    product_id          INTEGER REFERENCES products(id) ON DELETE SET NULL,
    sku                 TEXT    NOT NULL,
    product_name        TEXT    NOT NULL,
    unit_price_paisa    INTEGER NOT NULL CHECK (unit_price_paisa >= 0),
    quantity            INTEGER NOT NULL CHECK (quantity > 0),
    line_total_paisa    INTEGER NOT NULL CHECK (line_total_paisa = unit_price_paisa * quantity)
))"),
        QStringLiteral("CREATE INDEX idx_order_items_order ON order_items(order_id)"),
        QStringLiteral("CREATE INDEX idx_order_items_product ON order_items(product_id)"),
        // Order lines are a snapshot of what was bought; they must never change.
        QStringLiteral(R"(
CREATE TRIGGER trg_order_items_immutable
BEFORE UPDATE OF order_id, sku, product_name, unit_price_paisa, quantity, line_total_paisa ON order_items
BEGIN
    SELECT RAISE(ABORT, 'order items are immutable');
END)"),
        QStringLiteral(R"(
CREATE TABLE order_status_history (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    order_id    INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE,
    from_status TEXT,
    to_status   TEXT    NOT NULL,
    changed_by  INTEGER REFERENCES users(id) ON DELETE SET NULL,
    note        TEXT    NOT NULL DEFAULT '',
    changed_at  TEXT    NOT NULL
))"),
        QStringLiteral("CREATE INDEX idx_status_history_order ON order_status_history(order_id)"),
        QStringLiteral(R"(
CREATE TABLE inventory_movements (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    product_id      INTEGER NOT NULL REFERENCES products(id) ON DELETE CASCADE,
    quantity_change INTEGER NOT NULL CHECK (quantity_change <> 0),
    stock_after     INTEGER NOT NULL CHECK (stock_after >= 0),
    reason          TEXT    NOT NULL CHECK (reason IN ('initial', 'restock', 'adjustment',
                                                       'damaged', 'sale', 'cancel_restock')),
    note            TEXT    NOT NULL DEFAULT '',
    user_id         INTEGER REFERENCES users(id) ON DELETE SET NULL,
    created_at      TEXT    NOT NULL
))"),
        QStringLiteral("CREATE INDEX idx_movements_product ON inventory_movements(product_id)"),
        QStringLiteral("CREATE INDEX idx_movements_created ON inventory_movements(created_at)"),
        QStringLiteral(R"(
CREATE TABLE audit_logs (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER REFERENCES users(id) ON DELETE SET NULL,
    action      TEXT    NOT NULL,
    entity      TEXT    NOT NULL,
    entity_id   INTEGER,
    details     TEXT    NOT NULL DEFAULT '',
    created_at  TEXT    NOT NULL
))"),
        QStringLiteral("CREATE INDEX idx_audit_created ON audit_logs(created_at)"),
        QStringLiteral(R"(
CREATE TABLE app_settings (
    key     TEXT PRIMARY KEY,
    value   TEXT NOT NULL
))"),
        // Pricing rules. Change them here or in the app_settings table.
        QStringLiteral(R"(
INSERT INTO app_settings (key, value) VALUES
    ('shipping_flat_paisa',           '25000'),
    ('free_shipping_threshold_paisa', '500000'),
    ('tax_rate_bp',                   '0'),
    ('demo_data_loaded',              '0')
)"),
    };
}

} // namespace

Migrator::Migrator(QString connectionName)
    : m_connection(std::move(connectionName))
{
}

const QVector<Migration> &Migrator::migrations()
{
    static const QVector<Migration> all = {
        {1, QStringLiteral("initial_schema"), migration1()},
    };
    return all;
}

int Migrator::latestVersion()
{
    return migrations().isEmpty() ? 0 : migrations().last().version;
}

int Migrator::currentVersion() const
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")) || !q.next())
        return 0;
    return q.value(0).toInt();
}

Status Migrator::migrate()
{
    const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    if (auto st = sql::execRaw(db,
                               QStringLiteral("CREATE TABLE IF NOT EXISTS schema_migrations ("
                                              " version INTEGER PRIMARY KEY,"
                                              " name TEXT NOT NULL,"
                                              " applied_at TEXT NOT NULL)"),
                               "Migrator::migrate");
        !st)
        return st;

    const int current = currentVersion();
    if (current > latestVersion()) {
        qCritical() << "Database schema version" << current << "is newer than this build supports ("
                    << latestVersion() << ")";
        return Status::fail(QStringLiteral(
            "This database was created by a newer version of ShopHub. Please update the application."));
    }

    for (const Migration &m : migrations()) {
        if (m.version <= current)
            continue;

        Transaction tx(m_connection);
        if (auto st = tx.begin(); !st)
            return st;

        for (const QString &statement : m.statements) {
            if (auto st = sql::execRaw(db, statement, "Migrator::migrate"); !st) {
                qCritical().noquote() << "Migration" << m.version << m.name << "failed";
                return Status::fail(QStringLiteral("Database upgrade failed. See the log file for details."));
            }
        }

        QSqlQuery record(db);
        record.prepare(QStringLiteral(
            "INSERT INTO schema_migrations (version, name, applied_at) VALUES (:v, :n, :t)"));
        record.bindValue(QStringLiteral(":v"), m.version);
        record.bindValue(QStringLiteral(":n"), m.name);
        record.bindValue(QStringLiteral(":t"), sql::nowUtc());
        if (auto st = sql::exec(record, "Migrator::record"); !st)
            return st;

        if (auto st = tx.commit(); !st)
            return st;
        qInfo().noquote() << "Applied migration" << m.version << m.name;
    }
    return Status::ok();
}

} // namespace shophub
