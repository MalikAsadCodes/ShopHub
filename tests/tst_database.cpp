#include "TestSupport.h"

#include "database/Migrations.h"

using namespace shophub;

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
    }
    void cleanup() { m_b.reset(); }

    void migratedToLatestVersion()
    {
        Migrator m(m_b->ctx->connectionName());
        QCOMPARE(m.currentVersion(), Migrator::latestVersion());
        // Running again is a no-op, not an error.
        QVERIFY(m.migrate().isOk());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM schema_migrations")).toInt(),
                 Migrator::latestVersion());
    }

    void foreignKeysAreEnforced()
    {
        QCOMPARE(m_b->scalar(QStringLiteral("PRAGMA foreign_keys")).toInt(), 1);
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        const bool ok = q.exec(QStringLiteral(
            "INSERT INTO products (sku, name, category_id, price_paisa, created_at, updated_at) "
            "VALUES ('X-1', 'Orphan', 99999, 100, 'x', 'x')"));
        QVERIFY2(!ok, "insert with a missing category must fail");
    }

    void stockCannotGoNegative()
    {
        const qint64 id = m_b->addProduct(QStringLiteral("NEG-1"), 100, 2);
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        QVERIFY(!q.exec(QStringLiteral("UPDATE products SET stock = -1 WHERE id = %1").arg(id)));
        QCOMPARE(m_b->stockOf(id), 2);
    }

    void orderItemsAreImmutable()
    {
        const qint64 pid = m_b->addProduct(QStringLiteral("IMM-1"), 500, 5);
        QVERIFY(m_b->ctx->cart().add(m_b->customer, pid, 1).isOk());
        auto order = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY2(order.isOk(), qPrintable(order.error()));

        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        QVERIFY(!q.exec(QStringLiteral("UPDATE order_items SET unit_price_paisa = 1")));
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT unit_price_paisa FROM order_items")).toLongLong(), qint64(50000));
    }

    void orderTotalMustAddUp()
    {
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        const bool ok = q.exec(QStringLiteral(
            "INSERT INTO orders (order_number, user_id, status, subtotal_paisa, shipping_paisa, tax_paisa, "
            " total_paisa, payment_method, payment_status, ship_name, ship_phone, ship_line1, ship_city, "
            " checkout_token, created_at, updated_at) VALUES ('N1', %1, 'pending', 100, 0, 0, 999, 'cod', "
            " 'unpaid', 'a', 'b', 'c', 'd', 'tok', 'x', 'x')").arg(m_b->customer.userId()));
        QVERIFY2(!ok, "a wrong total must be rejected by the CHECK constraint");
    }

    void settingsSeeded()
    {
        QCOMPARE(m_b->ctx->pricing().rules().shippingFlat, Money::fromRupees(250));
        QCOMPARE(m_b->ctx->pricing().rules().freeShippingThreshold, Money::fromRupees(5000));
    }

private:
    std::unique_ptr<test::Backend> m_b;
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
