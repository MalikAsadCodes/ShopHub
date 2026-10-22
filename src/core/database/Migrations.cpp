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

// --------------------------------------------------------------------------
// Migration 2: Khata (credit ledger) for Pakistan Retail Edition.
//  - orders may now be paid "on account" (payment_method 'khata')
//  - khata_accounts: one credit limit per customer
//  - khata_entries: append-only ledger. amount > 0 raises what the customer
//    owes (a sale on credit), amount < 0 lowers it (payment, cancelled order).
//    The balance is the sum of the entries. It goes below zero only when a
//    khata order is cancelled after being paid: the shop then owes the
//    customer an advance, which is used up by the next khata order.
// --------------------------------------------------------------------------
QStringList migration2()
{
    return {
        // Rebuild orders with wider CHECK constraints (the recommended SQLite procedure).
        QStringLiteral(R"(
CREATE TABLE orders_new (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    order_number    TEXT    NOT NULL UNIQUE,
    user_id         INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    status          TEXT    NOT NULL CHECK (status IN ('pending', 'confirmed', 'processing',
                                                       'shipped', 'delivered', 'cancelled')),
    subtotal_paisa  INTEGER NOT NULL CHECK (subtotal_paisa >= 0),
    shipping_paisa  INTEGER NOT NULL CHECK (shipping_paisa >= 0),
    tax_paisa       INTEGER NOT NULL CHECK (tax_paisa >= 0),
    total_paisa     INTEGER NOT NULL CHECK (total_paisa = subtotal_paisa + shipping_paisa + tax_paisa),
    payment_method  TEXT    NOT NULL CHECK (payment_method IN ('cod', 'khata')),
    payment_status  TEXT    NOT NULL CHECK (payment_status IN ('unpaid', 'paid', 'on_account')),
    ship_name       TEXT    NOT NULL,
    ship_phone      TEXT    NOT NULL,
    ship_line1      TEXT    NOT NULL,
    ship_city       TEXT    NOT NULL,
    checkout_token  TEXT    NOT NULL UNIQUE,
    created_at      TEXT    NOT NULL,
    updated_at      TEXT    NOT NULL
))"),
        QStringLiteral(R"(
INSERT INTO orders_new (id, order_number, user_id, status, subtotal_paisa, shipping_paisa, tax_paisa,
                        total_paisa, payment_method, payment_status, ship_name, ship_phone, ship_line1,
                        ship_city, checkout_token, created_at, updated_at)
SELECT id, order_number, user_id, status, subtotal_paisa, shipping_paisa, tax_paisa,
       total_paisa, payment_method, payment_status, ship_name, ship_phone, ship_line1,
       ship_city, checkout_token, created_at, updated_at
FROM orders)"),
        QStringLiteral("DROP TABLE orders"),
        QStringLiteral("ALTER TABLE orders_new RENAME TO orders"),
        QStringLiteral("CREATE INDEX idx_orders_user ON orders(user_id)"),
        QStringLiteral("CREATE INDEX idx_orders_status ON orders(status)"),
        QStringLiteral("CREATE INDEX idx_orders_created ON orders(created_at)"),

        QStringLiteral(R"(
CREATE TABLE khata_accounts (
    user_id             INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE RESTRICT,
    credit_limit_paisa  INTEGER NOT NULL DEFAULT 0 CHECK (credit_limit_paisa >= 0),
    created_at          TEXT    NOT NULL,
    updated_at          TEXT    NOT NULL
))"),
        QStringLiteral(R"(
CREATE TABLE khata_entries (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id              INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    kind                 TEXT    NOT NULL CHECK (kind IN ('charge', 'payment', 'reversal', 'adjustment')),
    amount_paisa         INTEGER NOT NULL CHECK (amount_paisa <> 0),
    balance_after_paisa  INTEGER NOT NULL,
    order_id             INTEGER REFERENCES orders(id) ON DELETE RESTRICT,
    note                 TEXT    NOT NULL DEFAULT '',
    created_by           INTEGER REFERENCES users(id),
    created_at           TEXT    NOT NULL,
    CHECK ((kind = 'charge'   AND amount_paisa > 0 AND order_id IS NOT NULL)
        OR (kind = 'reversal' AND amount_paisa < 0 AND order_id IS NOT NULL)
        OR (kind = 'payment'  AND amount_paisa < 0)
        OR (kind = 'adjustment'))
))"),
        QStringLiteral("CREATE INDEX idx_khata_user ON khata_entries(user_id, id)"),
        // An order can be put on account once and reversed once.
        QStringLiteral("CREATE UNIQUE INDEX idx_khata_one_charge ON khata_entries(order_id) WHERE kind = 'charge'"),
        QStringLiteral("CREATE UNIQUE INDEX idx_khata_one_reversal ON khata_entries(order_id) WHERE kind = 'reversal'"),
        // A ledger is a history: entries are never edited or deleted, only corrected by new entries.
        QStringLiteral(R"(
CREATE TRIGGER trg_khata_entries_no_update
BEFORE UPDATE ON khata_entries
BEGIN
    SELECT RAISE(ABORT, 'khata entries are immutable');
END)"),
        QStringLiteral(R"(
CREATE TRIGGER trg_khata_entries_no_delete
BEFORE DELETE ON khata_entries
BEGIN
    SELECT RAISE(ABORT, 'khata entries cannot be deleted');
END)"),
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
        {2, QStringLiteral("khata_ledger"), migration2(), true},
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

        // Table rebuilds need foreign keys off. The pragma is ignored inside a
        // transaction, so switch it before BEGIN and always switch it back on.
        struct ForeignKeysGuard
        {
            const QSqlDatabase &db;
            bool active = false;
            ~ForeignKeysGuard()
            {
                if (active)
                    sql::execRaw(db, QStringLiteral("PRAGMA foreign_keys = ON"), "Migrator::restoreForeignKeys");
            }
        } fkGuard{db};
        if (m.rebuildsTables) {
            if (auto st = sql::execRaw(db, QStringLiteral("PRAGMA foreign_keys = OFF"), "Migrator::migrate"); !st)
                return st;
            fkGuard.active = true;
        }

        Transaction tx(m_connection);
        if (auto st = tx.begin(); !st)
            return st;

        for (const QString &statement : m.statements) {
            if (auto st = sql::execRaw(db, statement, "Migrator::migrate"); !st) {
                qCritical().noquote() << "Migration" << m.version << m.name << "failed";
                return Status::fail(QStringLiteral("Database upgrade failed. See the log file for details."));
            }
        }

        if (m.rebuildsTables) {
            // Every reference must still point at a real row before we commit.
            QSqlQuery check(db);
            if (!check.exec(QStringLiteral("PRAGMA foreign_key_check")))
                return sql::failure(check, "Migrator::foreignKeyCheck");
            if (check.next()) {
                qCritical().noquote() << "Migration" << m.version << m.name
                                      << "broke a foreign key in table" << check.value(0).toString();
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
