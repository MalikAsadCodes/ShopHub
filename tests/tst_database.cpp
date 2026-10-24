#include "TestSupport.h"

#include "database/Migrations.h"
#include "database/Transaction.h"
#include "repositories/ProductRepository.h"

#include <QRandomGenerator>
#include <QSqlError>

#include <algorithm>
#include <iterator>

using namespace shophub;

namespace {

PriceTier tier(int minQuantity, int discountBp)
{
    PriceTier t;
    t.minQuantity = minQuantity;
    t.discountBp = discountBp;
    return t;
}

} // namespace

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

    // ------------------------------------------------------------------ wholesale price tiers (migration 3)
    void priceTierTableRules()
    {
        const qint64 pid = m_b->addProduct(QStringLiteral("TIER-1"), 1250, 100);
        QVERIFY(insertTier(pid, 2, 1));        // smallest allowed values
        QVERIFY(insertTier(pid, 99, 9000));    // largest allowed values
        QVERIFY(!insertTier(pid, 1, 500));     // a tier starts at 2 units or more
        QVERIFY(!insertTier(pid, 100, 500));   // a cart line holds at most 99
        QVERIFY(!insertTier(pid, 12, 0));      // the discount must be above 0%
        QVERIFY(!insertTier(pid, 12, -500));
        QVERIFY(!insertTier(pid, 12, 9001));   // and at most 90%
        QVERIFY(!insertTier(pid, 2, 300));     // one tier per starting quantity
        QVERIFY(!insertTier(424242, 12, 500)); // the product must exist
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM product_price_tiers")).toInt(), 2);
    }

    void priceTiersSaveAndLoad()
    {
        const qint64 a = m_b->addProduct(QStringLiteral("TIER-A"), 1250, 100);
        const qint64 b = m_b->addProduct(QStringLiteral("TIER-B"), 500, 100);
        ProductRepository repo(m_b->ctx->connectionName());

        // No tiers yet: an empty list, not an error.
        auto none = repo.tiersFor(a);
        QVERIFY(none.isOk());
        QVERIFY(none.value().isEmpty());

        // Saved in any order, loaded sorted by starting quantity.
        QVERIFY(saveTiers(a, {tier(50, 1000), tier(12, 500), tier(6, 200)}).isOk());
        QVERIFY(saveTiers(b, {tier(10, 300)}).isOk());
        auto loaded = repo.tiersFor(a);
        QVERIFY(loaded.isOk());
        QCOMPARE(loaded.value().size(), 3);
        QCOMPARE(loaded.value().at(0).minQuantity, 6);
        QCOMPARE(loaded.value().at(0).discountBp, 200);
        QCOMPARE(loaded.value().at(1).minQuantity, 12);
        QCOMPARE(loaded.value().at(1).discountBp, 500);
        QCOMPARE(loaded.value().at(2).minQuantity, 50);
        QCOMPARE(loaded.value().at(2).discountBp, 1000);

        // Saving again replaces the whole list instead of adding to it.
        QVERIFY(saveTiers(a, {tier(24, 800)}).isOk());
        loaded = repo.tiersFor(a);
        QVERIFY(loaded.isOk());
        QCOMPARE(loaded.value().size(), 1);
        QCOMPARE(loaded.value().at(0).minQuantity, 24);
        QCOMPARE(loaded.value().at(0).discountBp, 800);

        // An empty list removes the tiers.
        QVERIFY(saveTiers(a, {}).isOk());
        QVERIFY(repo.tiersFor(a).value().isEmpty());

        // The other product was never touched.
        auto other = repo.tiersFor(b);
        QVERIFY(other.isOk());
        QCOMPARE(other.value().size(), 1);
        QCOMPARE(other.value().at(0).minQuantity, 10);
    }

    void failedTierSaveKeepsOldTiers()
    {
        const qint64 pid = m_b->addProduct(QStringLiteral("TIER-RB"), 1250, 100);
        QVERIFY(saveTiers(pid, {tier(12, 500), tier(50, 1000)}).isOk());

        // The old tiers are already deleted when the second new tier breaks a
        // CHECK rule. The rollback must bring them back.
        QVERIFY(!saveTiers(pid, {tier(6, 200), tier(100, 300)}).isOk());

        auto loaded = ProductRepository(m_b->ctx->connectionName()).tiersFor(pid);
        QVERIFY(loaded.isOk());
        QCOMPARE(loaded.value().size(), 2);
        QCOMPARE(loaded.value().at(0).minQuantity, 12);
        QCOMPARE(loaded.value().at(1).minQuantity, 50);
    }

    void priceTiersAreDeletedWithTheProduct()
    {
        const qint64 pid = m_b->addProduct(QStringLiteral("TIER-DEL"), 100, 5);
        QVERIFY(saveTiers(pid, {tier(12, 500)}).isOk());

        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        QVERIFY2(q.exec(QStringLiteral("DELETE FROM products WHERE id = %1").arg(pid)), qPrintable(q.lastError().text()));
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM product_price_tiers WHERE product_id = %1").arg(pid))
                     .toInt(),
                 0);
    }

    // ------------------------------------------------------------------ tier rules in the database (migration 4)
    void tierTriggersEnforceTheRules()
    {
        const qint64 pid = m_b->addProduct(QStringLiteral("TRG-1"), 1250, 100);
        const qint64 other = m_b->addProduct(QStringLiteral("TRG-2"), 1250, 100);

        QVERIFY(insertTier(pid, 12, 500));
        QVERIFY(!insertTier(pid, 50, 400)); // more units, smaller discount
        QVERIFY(!insertTier(pid, 6, 600));  // fewer units, bigger discount
        QVERIFY(insertTier(pid, 24, 500));  // an equal discount is fine
        QVERIFY(insertTier(pid, 6, 200));
        QVERIFY(insertTier(pid, 50, 1000));
        QVERIFY(insertTier(pid, 99, 1000));
        QVERIFY(!insertTier(pid, 98, 1000)); // a 6th tier
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM product_price_tiers WHERE product_id = %1").arg(pid))
                     .toInt(),
                 5);

        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        // UPDATE is covered too.
        QVERIFY(!q.exec(QStringLiteral("UPDATE product_price_tiers SET discount_bp = 9000 "
                                       "WHERE product_id = %1 AND min_quantity = 12").arg(pid)));
        QVERIFY2(q.exec(QStringLiteral("UPDATE product_price_tiers SET discount_bp = 300 "
                                       "WHERE product_id = %1 AND min_quantity = 12").arg(pid)),
                 qPrintable(q.lastError().text())); // still between 6+ (2%) and 24+ (5%)

        // Moving a tier onto a product that already has 5 is refused; onto a free one it is fine.
        QVERIFY(insertTier(other, 2, 100));
        QVERIFY(insertTier(other, 3, 100));
        QVERIFY(insertTier(other, 4, 100));
        QVERIFY(insertTier(other, 5, 100));
        QVERIFY(insertTier(other, 7, 100));
        QVERIFY(!q.exec(QStringLiteral("UPDATE product_price_tiers SET product_id = %1 "
                                       "WHERE product_id = %2 AND min_quantity = 99").arg(other).arg(pid)));
        QVERIFY(q.exec(QStringLiteral("DELETE FROM product_price_tiers WHERE product_id = %1").arg(other)));
        QVERIFY2(q.exec(QStringLiteral("UPDATE product_price_tiers SET product_id = %1 "
                                       "WHERE product_id = %2 AND min_quantity = 99").arg(other).arg(pid)),
                 qPrintable(q.lastError().text()));
    }

    void replaceTiersIsAllOrNothingWithoutATransaction()
    {
        const qint64 pid = m_b->addProduct(QStringLiteral("SVP-1"), 1250, 100);
        ProductRepository repo(m_b->ctx->connectionName());
        QVERIFY(saveTiers(pid, {tier(12, 500), tier(50, 1000)}).isOk());

        // No Transaction here: the savepoint alone must undo the delete.
        QVERIFY(!repo.replaceTiers(pid, {tier(6, 200), tier(100, 300)}).isOk());
        auto kept = repo.tiersFor(pid);
        QVERIFY(kept.isOk());
        QCOMPARE(kept.value().size(), 2);
        QCOMPARE(kept.value().at(0).minQuantity, 12);
        QCOMPARE(kept.value().at(1).minQuantity, 50);

        // A good save without a Transaction is kept (the savepoint commits it).
        QVERIFY(repo.replaceTiers(pid, {tier(24, 700)}).isOk());
        auto saved = repo.tiersFor(pid);
        QCOMPARE(saved.value().size(), 1);
        QCOMPARE(saved.value().at(0).minQuantity, 24);

        // And inside a Transaction that is rolled back, nothing is kept.
        {
            Transaction tx(m_b->ctx->connectionName());
            QVERIFY(tx.begin().isOk());
            QVERIFY(repo.replaceTiers(pid, {tier(2, 100)}).isOk());
        } // rolled back here
        QCOMPARE(repo.tiersFor(pid).value().at(0).minQuantity, 24);
    }

    void databaseAndValidateAgree()
    {
        // The rules live in two places (pricetier::validate and the database).
        // Run the same tier lists through both and make sure they never disagree.
        const qint64 pid = m_b->addProduct(QStringLiteral("PAR-1"), 1250, 100);
        ProductRepository repo(m_b->ctx->connectionName());
        const int quantities[] = {1, 2, 3, 6, 12, 24, 50, 99, 100};
        const int discounts[] = {-5, 0, 1, 200, 500, 1000, 9000, 9001};
        QRandomGenerator gen(20261023); // fixed seed: the same lists on every run
        int accepted = 0;
        int refused = 0;

        // Refused saves log an SQL warning each; keep the test output readable.
        const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});
        for (int round = 0; round < 400; ++round) {
            const int n = int(gen.bounded(7)); // 0-6 tiers
            QVector<int> qs;
            QVector<int> ds;
            for (int i = 0; i < n; ++i) {
                qs << quantities[gen.bounded(int(std::size(quantities)))];
                ds << discounts[gen.bounded(int(std::size(discounts)))];
            }
            if (gen.bounded(2) == 0) { // half the lists: pair quantities and discounts in order, so most are valid
                std::sort(qs.begin(), qs.end());
                std::sort(ds.begin(), ds.end());
            }
            QVector<PriceTier> tiers;
            for (int i = 0; i < n; ++i)
                tiers << tier(qs.at(i), ds.at(i));
            std::shuffle(tiers.begin(), tiers.end(), gen); // the saving order must not matter

            const bool valid = pricetier::validate(tiers).isEmpty();
            Transaction tx(m_b->ctx->connectionName());
            const bool txStarted = tx.begin().isOk();
            const bool saved = txStarted && repo.replaceTiers(pid, tiers).isOk();
            tx.rollback();
            if (!txStarted || valid != saved) {
                qInstallMessageHandler(previous);
                QString list;
                for (const PriceTier &t : tiers)
                    list += QStringLiteral(" (%1, %2)").arg(t.minQuantity).arg(t.discountBp);
                QFAIL(qPrintable(QStringLiteral("validate says %1, database says %2 for:%3")
                                     .arg(valid ? QStringLiteral("ok") : QStringLiteral("refused"),
                                          saved ? QStringLiteral("ok") : QStringLiteral("refused"), list)));
            }
            ++(valid ? accepted : refused);
        }
        qInstallMessageHandler(previous);
        // Both kinds of list were really tried.
        QVERIFY2(accepted >= 30, qPrintable(QStringLiteral("only %1 valid lists tried").arg(accepted)));
        QVERIFY2(refused >= 30, qPrintable(QStringLiteral("only %1 invalid lists tried").arg(refused)));
    }

    // ------------------------------------------------------------------ upgrading a v2 database
    void upgradesAVersion2Database()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("v2.db"));
        {
            // Build a database exactly as ShopHub v2.0.0-preview1 left it (migrations 1 and 2).
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("v2"));
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE schema_migrations (version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                                          "applied_at TEXT NOT NULL)")));
            for (int i = 0; i < 2; ++i) {
                const Migration &m = Migrator::migrations().at(i);
                for (const QString &statement : m.statements)
                    QVERIFY2(q.exec(statement), qPrintable(q.lastError().text()));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO schema_migrations VALUES (%1, '%2', '2026-10-18T00:00:00Z')")
                                   .arg(m.version)
                                   .arg(m.name)));
            }
            QVERIFY(q.exec(QStringLiteral("INSERT INTO categories (id, name, created_at) VALUES (1, 'Grocery', 'n')")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO products (id, sku, name, category_id, price_paisa, stock, "
                                          "created_at, updated_at) VALUES (3, 'GR-1', 'Cooking oil 5L', 1, 125000, "
                                          "40, 'n', 'n')")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("v2"));

        AppConfig cfg;
        cfg.databasePath = path;
        cfg.imageDirectory = dir.filePath(QStringLiteral("images"));
        cfg.passwordIterations = 1000;
        auto created = AppContext::create(cfg);
        QVERIFY2(created.isOk(), qPrintable(created.error()));
        auto ctx = created.takeValue();
        QSqlQuery q(QSqlDatabase::database(ctx->connectionName(), false));
        auto scalar = [&q](const QString &sql) {
            return q.exec(sql) && q.next() ? q.value(0) : QVariant();
        };

        QCOMPARE(scalar(QStringLiteral("SELECT MAX(version) FROM schema_migrations")).toInt(), Migrator::latestVersion());
        QCOMPARE(scalar(QStringLiteral("SELECT name FROM schema_migrations WHERE version = 3")).toString(),
                 QStringLiteral("wholesale_price_tiers"));
        QCOMPARE(scalar(QStringLiteral("PRAGMA foreign_keys")).toInt(), 1);
        QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_key_check")));
        QVERIFY(!q.next());
        QVERIFY(q.exec(QStringLiteral("PRAGMA integrity_check")) && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("ok"));

        // The old product survived unchanged, with no tiers yet...
        QCOMPARE(scalar(QStringLiteral("SELECT price_paisa FROM products WHERE id = 3")).toLongLong(), qint64(125000));
        QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM product_price_tiers")).toInt(), 0);

        // ...and can now get quantity prices.
        ProductRepository repo(ctx->connectionName());
        {
            Transaction tx(ctx->connectionName());
            QVERIFY(tx.begin().isOk());
            QVERIFY(repo.replaceTiers(3, {tier(12, 500)}).isOk());
            QVERIFY(tx.commit().isOk());
        }
        auto loaded = repo.tiersFor(3);
        QVERIFY(loaded.isOk());
        QCOMPARE(loaded.value().size(), 1);
        QCOMPARE(loaded.value().at(0).discountBp, 500);
    }

private:
    // Raw insert, to test the table's own rules without the repository.
    bool insertTier(qint64 productId, int minQuantity, int discountBp)
    {
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        return q.exec(QStringLiteral("INSERT INTO product_price_tiers (product_id, min_quantity, discount_bp) "
                                     "VALUES (%1, %2, %3)")
                          .arg(productId)
                          .arg(minQuantity)
                          .arg(discountBp));
    }

    // Saves tiers the way the catalog service will: inside one write transaction.
    Status saveTiers(qint64 productId, const QVector<PriceTier> &tiers)
    {
        Transaction tx(m_b->ctx->connectionName());
        if (auto st = tx.begin(); !st)
            return st;
        if (auto st = ProductRepository(m_b->ctx->connectionName()).replaceTiers(productId, tiers); !st)
            return st; // the Transaction destructor rolls back
        return tx.commit();
    }

    std::unique_ptr<test::Backend> m_b;
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
