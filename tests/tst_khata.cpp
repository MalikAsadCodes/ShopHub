#include "TestSupport.h"

#include "database/Migrations.h"
#include "database/SqlUtil.h"
#include "services/DemoSeeder.h"

#include <QSqlError>

using namespace shophub;

// Khata (credit ledger): limits, checkout on credit, cancellation, payments,
// adjustments, permissions, ledger integrity and upgrading a v1 database.
class TestKhata : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<test::Backend> m_b;
    qint64 m_kettle = 0; // Rs 2,000 -> Rs 2,250 with delivery
    qint64 m_tv = 0;     // Rs 60,000

    KhataService &khata() { return m_b->ctx->khata(); }
    Money balance() { return khata().account(m_b->admin, m_b->customer.userId()).value().balance; }
    qint64 count(const QString &sql) { return m_b->scalar(sql).toLongLong(); }

    Result<Order> buy(qint64 productId, int qty, const QString &method)
    {
        auto added = m_b->ctx->cart().add(m_b->customer, productId, qty);
        if (!added)
            return added;
        CheckoutRequest req = m_b->checkoutRequest();
        req.paymentMethod = method;
        return m_b->ctx->checkout().placeOrder(m_b->customer, req);
    }

    // The running balance on the newest entry must equal the sum of all entries.
    void verifyLedger(qint64 userId)
    {
        const qint64 sum = count(QStringLiteral("SELECT COALESCE(SUM(amount_paisa), 0) FROM khata_entries WHERE user_id = %1").arg(userId));
        const qint64 last = count(QStringLiteral("SELECT COALESCE((SELECT balance_after_paisa FROM khata_entries "
                                                 "WHERE user_id = %1 ORDER BY id DESC LIMIT 1), 0)").arg(userId));
        QCOMPARE(last, sum);
    }

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
        m_kettle = m_b->addProduct(QStringLiteral("KET-1"), 2000, 50, QStringLiteral("Kettle"));
        m_tv = m_b->addProduct(QStringLiteral("TV-1"), 60000, 5, QStringLiteral("Television"));
    }
    void cleanup() { m_b.reset(); }

    // ------------------------------------------------------------------ limits & permissions
    void newCustomerHasNoKhata()
    {
        auto mine = khata().myAccount(m_b->customer);
        QVERIFY(mine.isOk());
        QCOMPARE(mine.value().creditLimit, Money());
        QCOMPARE(mine.value().balance, Money());
        QCOMPARE(mine.value().available(), Money());
        QVERIFY(khata().myEntries(m_b->customer).value().isEmpty());
    }

    void onlyAdminsManageKhata()
    {
        const qint64 me = m_b->customer.userId();
        QVERIFY(!khata().setLimit(m_b->customer, me, Money::fromRupees(100000)).isOk());
        QVERIFY(!khata().recordPayment(m_b->customer, me, Money::fromRupees(1), QString()).isOk());
        QVERIFY(!khata().adjust(m_b->customer, me, Money::fromRupees(-1), QStringLiteral("x")).isOk());
        QVERIFY(!khata().accounts(m_b->customer).isOk());
        QVERIFY(!khata().entries(m_b->customer, me).isOk());
        QVERIFY(!khata().myAccount(m_b->admin).isOk());   // admins have no khata of their own
        QVERIFY(!khata().myAccount(Session()).isOk());     // signed out
    }

    void limitRules()
    {
        const qint64 me = m_b->customer.userId();
        QVERIFY(!khata().setLimit(m_b->admin, me, Money::fromRupees(-5)).isOk());
        QVERIFY(!khata().setLimit(m_b->admin, me, Money::fromRupees(KhataService::kMaxLimitRupees + 1)).isOk());
        QVERIFY(!khata().setLimit(m_b->admin, m_b->admin.userId(), Money::fromRupees(100)).isOk()); // not a customer
        QVERIFY(!khata().setLimit(m_b->admin, 999999, Money::fromRupees(100)).isOk());
        QVERIFY(khata().setLimit(m_b->admin, me, Money::fromRupees(10000)).isOk());
        QVERIFY(khata().setLimit(m_b->admin, me, Money::fromRupees(15000)).isOk()); // update, not duplicate
        QCOMPARE(khata().myAccount(m_b->customer).value().creditLimit, Money::fromRupees(15000));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_accounts")), 1);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM audit_logs WHERE action = 'khata_limit_set'")), 2);
    }

    // ------------------------------------------------------------------ checkout
    void khataNeedsALimit()
    {
        auto r = buy(m_kettle, 1, payment::kKhata);
        QVERIFY(!r.isOk());
        QVERIFY(r.error().contains(QLatin1String("not open")));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders")), 0);
        QCOMPARE(m_b->stockOf(m_kettle), 50);
        QCOMPARE(m_b->ctx->cart().itemCount(m_b->customer).value(), 1); // cart kept
    }

    void unknownPaymentMethodRefused()
    {
        auto r = buy(m_kettle, 1, QStringLiteral("bitcoin"));
        QVERIFY(!r.isOk());
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders")), 0);
    }

    void khataOrderGoesOnTheLedger()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        auto r = buy(m_kettle, 2, payment::kKhata);
        QVERIFY2(r.isOk(), qPrintable(r.error()));
        const Order &o = r.value();
        QCOMPARE(o.paymentMethod, payment::kKhata);
        QCOMPARE(o.paymentStatus, payment::kOnAccount);
        QCOMPARE(o.total, Money::fromRupees(4250));

        QCOMPARE(balance(), Money::fromRupees(4250));
        auto entries = khata().myEntries(m_b->customer);
        QCOMPARE(entries.value().size(), 1);
        const KhataEntry &e = entries.value().first();
        QCOMPARE(e.kind, KhataKind::Charge);
        QCOMPARE(e.amount, o.total);
        QCOMPARE(e.balanceAfter, o.total);
        QCOMPARE(e.orderId, o.id);
        QCOMPARE(e.orderNumber, o.orderNumber);
        QCOMPARE(khata().myAccount(m_b->customer).value().available(), Money::fromRupees(5750));
        QCOMPARE(m_b->ctx->orders().history(m_b->customer, o.id).value().first().note, QStringLiteral("Order placed (Khata)"));
        verifyLedger(m_b->customer.userId());
    }

    void codOrderDoesNotTouchKhata()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        auto r = buy(m_kettle, 1, payment::kCashOnDelivery);
        QVERIFY(r.isOk());
        QCOMPARE(r.value().paymentStatus, payment::kUnpaid);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries")), 0);
    }

    void overLimitIsRefusedAndNothingChanges()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(5000)).isOk());
        QVERIFY(buy(m_kettle, 1, payment::kKhata).isOk()); // Rs 2,250 -> Rs 2,750 left
        auto r = buy(m_kettle, 2, payment::kKhata);        // Rs 4,250 > Rs 2,750
        QVERIFY(!r.isOk());
        QVERIFY2(r.error().contains(QLatin1String("Rs 2,750.00")), qPrintable(r.error()));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders")), 1);
        QCOMPARE(balance(), Money::fromRupees(2250));
        QCOMPARE(m_b->stockOf(m_kettle), 49);
        QCOMPARE(m_b->ctx->cart().itemCount(m_b->customer).value(), 2);

        // Exactly up to the limit is fine.
        QVERIFY(m_b->ctx->cart().clear(m_b->customer).isOk());
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(6500)).isOk());
        QVERIFY(buy(m_kettle, 2, payment::kKhata).isOk());
        QCOMPARE(balance(), Money::fromRupees(6500));
        QCOMPARE(khata().myAccount(m_b->customer).value().available(), Money());
    }

    void loweringTheLimitStopsNewCredit()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        QVERIFY(buy(m_kettle, 2, payment::kKhata).isOk());
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(1000)).isOk()); // below balance
        auto mine = khata().myAccount(m_b->customer).value();
        QVERIFY(mine.overLimit());
        QCOMPARE(mine.available(), Money());
        QVERIFY(!buy(m_kettle, 1, payment::kKhata).isOk());
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money()).isOk()); // close the khata
        QVERIFY(!buy(m_kettle, 1, payment::kKhata).isOk());
        QCOMPARE(balance(), Money::fromRupees(4250)); // still owes what was bought
    }

    void failedCheckoutLeavesNoLedgerEntry()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        m_b->ctx->checkout().setFaultInjectorForTesting([](const QString &stage) {
            return stage == QLatin1String("before_commit") ? Status::fail(QStringLiteral("simulated crash")) : Status::ok();
        });
        QVERIFY(!buy(m_kettle, 1, payment::kKhata).isOk());
        m_b->ctx->checkout().setFaultInjectorForTesting(nullptr);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders")), 0);
        QCOMPARE(balance(), Money());
    }

    void duplicateSubmissionChargesOnce()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk());
        CheckoutRequest req = m_b->checkoutRequest();
        req.paymentMethod = payment::kKhata;
        auto first = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        auto second = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(first.isOk() && second.isOk());
        QCOMPARE(first.value().id, second.value().id);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries")), 1);
        QCOMPARE(balance(), Money::fromRupees(2250));
    }

    // ------------------------------------------------------------------ order life cycle
    void deliveredKhataOrderStaysOnKhata()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        const Order onKhata = buy(m_kettle, 1, payment::kKhata).value();
        const Order cod = buy(m_kettle, 1, payment::kCashOnDelivery).value();
        for (qint64 id : {onKhata.id, cod.id}) {
            for (OrderStatus s : {OrderStatus::Confirmed, OrderStatus::Processing, OrderStatus::Shipped, OrderStatus::Delivered})
                QVERIFY(m_b->ctx->orders().advance(m_b->admin, id, s).isOk());
        }
        QCOMPARE(m_b->ctx->orders().details(m_b->admin, onKhata.id).value().paymentStatus, payment::kOnAccount);
        QCOMPARE(m_b->ctx->orders().details(m_b->admin, cod.id).value().paymentStatus, payment::kPaid);
        QCOMPARE(balance(), Money::fromRupees(2250));
    }

    void cancellingTakesTheOrderOffTheKhata()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        const Order keep = buy(m_kettle, 1, payment::kKhata).value();
        const Order drop = buy(m_kettle, 2, payment::kKhata).value();
        QCOMPARE(balance(), Money::fromRupees(6500));

        QVERIFY(m_b->ctx->orders().cancel(m_b->customer, drop.id, QStringLiteral("Changed my mind")).isOk());
        QCOMPARE(balance(), keep.total);
        QCOMPARE(m_b->stockOf(m_kettle), 49);
        QVERIFY(!m_b->ctx->orders().cancel(m_b->admin, drop.id, QString()).isOk()); // already cancelled
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE kind = 'reversal'")), 1);

        auto entries = khata().myEntries(m_b->customer).value();
        QCOMPARE(entries.first().kind, KhataKind::Reversal); // newest first
        QCOMPARE(entries.first().amount, Money() - drop.total);
        QCOMPARE(entries.first().orderNumber, drop.orderNumber);
        verifyLedger(m_b->customer.userId());

        // The credit is free again.
        QCOMPARE(khata().myAccount(m_b->customer).value().available(), Money::fromRupees(10000) - keep.total);
    }

    void cancelAfterPayingLeavesAnAdvance()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        const Order o = buy(m_kettle, 1, payment::kKhata).value();
        QVERIFY(khata().recordPayment(m_b->admin, m_b->customer.userId(), o.total, QStringLiteral("Cash")).isOk());
        QVERIFY(m_b->ctx->orders().cancel(m_b->admin, o.id, QStringLiteral("Out of stock at supplier")).isOk());
        QCOMPARE(balance(), Money() - o.total); // the shop owes the customer
        QCOMPARE(khata().myAccount(m_b->customer).value().available(), Money::fromRupees(10000) + o.total);
        QCOMPARE(m_b->ctx->dashboard().stats(m_b->admin).value().khataOutstanding, Money());
        // Nothing to pay, and no write-off below zero.
        QVERIFY(!khata().recordPayment(m_b->admin, m_b->customer.userId(), Money::fromRupees(1), QString()).isOk());
        // The next khata order uses the advance first.
        QVERIFY(buy(m_kettle, 2, payment::kKhata).isOk());
        QCOMPARE(balance(), Money::fromRupees(4250) - o.total);
        verifyLedger(m_b->customer.userId());
    }

    // ------------------------------------------------------------------ payments & adjustments
    void paymentRules()
    {
        const qint64 me = m_b->customer.userId();
        QVERIFY(khata().setLimit(m_b->admin, me, Money::fromRupees(10000)).isOk());
        QVERIFY(!khata().recordPayment(m_b->admin, me, Money::fromRupees(100), QString()).isOk()); // owes nothing
        QVERIFY(buy(m_kettle, 2, payment::kKhata).isOk()); // Rs 4,250

        QVERIFY(!khata().recordPayment(m_b->admin, me, Money(), QString()).isOk());
        QVERIFY(!khata().recordPayment(m_b->admin, me, Money::fromRupees(-10), QString()).isOk());
        QVERIFY(!khata().recordPayment(m_b->admin, me, Money::fromRupees(4251), QString()).isOk()); // more than owed
        QVERIFY(!khata().recordPayment(m_b->admin, me, Money::fromRupees(1), QString(201, QLatin1Char('x'))).isOk());

        auto r = khata().recordPayment(m_b->admin, me, Money::fromRupees(1250), QStringLiteral("Easypaisa"));
        QVERIFY(r.isOk());
        QCOMPARE(r.value(), Money::fromRupees(3000));
        auto r2 = khata().recordPayment(m_b->admin, me, Money::fromRupees(3000), QStringLiteral("Cash"));
        QCOMPARE(r2.value(), Money());

        auto entries = khata().entries(m_b->admin, me).value();
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[1].kind, KhataKind::Payment);
        QCOMPARE(entries[1].amount, Money::fromRupees(-1250));
        QCOMPARE(entries[1].note, QStringLiteral("Easypaisa"));
        QCOMPARE(entries[1].createdBy, QStringLiteral("Admin User"));
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM audit_logs WHERE action = 'khata_payment'")), 2);
        verifyLedger(me);
    }

    void adjustmentRules()
    {
        const qint64 me = m_b->customer.userId();
        QVERIFY(khata().setLimit(m_b->admin, me, Money::fromRupees(10000)).isOk());
        QVERIFY(buy(m_kettle, 1, payment::kKhata).isOk()); // Rs 2,250

        QVERIFY(!khata().adjust(m_b->admin, me, Money::fromRupees(-50), QString()).isOk());            // note required
        QVERIFY(!khata().adjust(m_b->admin, me, Money(), QStringLiteral("nothing")).isOk());
        QVERIFY(!khata().adjust(m_b->admin, me, Money::fromRupees(-2251), QStringLiteral("too much")).isOk());

        QCOMPARE(khata().adjust(m_b->admin, me, Money::fromRupees(-250), QStringLiteral("Free delivery for a regular")).value(),
                 Money::fromRupees(2000));
        QCOMPARE(khata().adjust(m_b->admin, me, Money::fromRupees(100), QStringLiteral("Bag charge missed")).value(),
                 Money::fromRupees(2100));
        QCOMPARE(khata().myEntries(m_b->customer).value().first().kind, KhataKind::Adjustment);
        verifyLedger(me);
    }

    void customersSeeOnlyTheirOwnKhata()
    {
        auto other = m_b->ctx->auth().registerCustomer(QStringLiteral("Other Person"), QStringLiteral("other@test.pk"),
                                                        QStringLiteral("OtherPass1"));
        QVERIFY(other.isOk());
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        QVERIFY(buy(m_kettle, 1, payment::kKhata).isOk());
        QVERIFY(khata().myEntries(other.value()).value().isEmpty());
        QCOMPARE(khata().myAccount(other.value()).value().balance, Money());
    }

    void adminListAndDashboard()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        QVERIFY(buy(m_kettle, 1, payment::kKhata).isOk());
        auto other = m_b->ctx->auth().registerCustomer(QStringLiteral("Zara Ali"), QStringLiteral("zara@test.pk"),
                                                        QStringLiteral("ZaraPass12"));
        QVERIFY(other.isOk());

        auto all = khata().accounts(m_b->admin);
        QCOMPARE(all.value().size(), 2);                          // every customer, khata or not
        QCOMPARE(all.value().first().userId, m_b->customer.userId()); // biggest balance first
        QVERIFY(all.value().first().lastActivity.isValid());
        QVERIFY(!all.value().last().lastActivity.isValid());
        QCOMPARE(khata().accounts(m_b->admin, QString(), true).value().size(), 1);
        QCOMPARE(khata().accounts(m_b->admin, QStringLiteral("zara")).value().size(), 1);
        QCOMPARE(khata().accounts(m_b->admin, QStringLiteral("100%_")).value().size(), 0);

        const DashboardStats s = m_b->ctx->dashboard().stats(m_b->admin).value();
        QCOMPARE(s.khataOutstanding, Money::fromRupees(2250));
        QCOMPARE(s.khataDebtors, 1);
    }

    // ------------------------------------------------------------------ database rules
    void ledgerCannotBeEditedOrDeleted()
    {
        QVERIFY(khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        QVERIFY(buy(m_kettle, 1, payment::kKhata).isOk());
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        QVERIFY(!q.exec(QStringLiteral("UPDATE khata_entries SET amount_paisa = 1")));
        QVERIFY(!q.exec(QStringLiteral("UPDATE khata_entries SET created_at = '2020-01-01T00:00:00Z'")));
        QVERIFY(!q.exec(QStringLiteral("DELETE FROM khata_entries")));
        QCOMPARE(balance(), Money::fromRupees(2250));
    }

    void databaseRejectsBadEntries()
    {
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        const qint64 me = m_b->customer.userId();
        auto insert = [&](const QString &kind, qint64 amount, const QString &order) {
            return q.exec(QStringLiteral("INSERT INTO khata_entries (user_id, kind, amount_paisa, balance_after_paisa, "
                                         "order_id, created_at) VALUES (%1, '%2', %3, 0, %4, '2026-01-01T00:00:00Z')")
                              .arg(me).arg(kind).arg(amount).arg(order));
        };
        QVERIFY(!insert(QStringLiteral("charge"), 100, QStringLiteral("NULL")));   // a charge needs an order
        QVERIFY(!insert(QStringLiteral("payment"), 100, QStringLiteral("NULL")));  // a payment lowers the balance
        QVERIFY(!insert(QStringLiteral("adjustment"), 0, QStringLiteral("NULL"))); // never zero
        QVERIFY(!insert(QStringLiteral("gift"), -5, QStringLiteral("NULL")));      // unknown kind
        QVERIFY(!insert(QStringLiteral("charge"), 100, QStringLiteral("424242"))); // order must exist
        QVERIFY(!q.exec(QStringLiteral("INSERT INTO orders (order_number, user_id, status, subtotal_paisa, shipping_paisa, "
                                       "tax_paisa, total_paisa, payment_method, payment_status, ship_name, ship_phone, "
                                       "ship_line1, ship_city, checkout_token, created_at, updated_at) VALUES "
                                       "('X', %1, 'pending', 1, 0, 0, 1, 'card', 'unpaid', 'a', 'b', 'c', 'd', 't', 'n', 'n')")
                             .arg(me)));
    }

    // ------------------------------------------------------------------ upgrading a v1 database
    void upgradesAVersion1Database()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("v1.db"));
        {
            // Build a database exactly as ShopHub v1 left it, with one COD order.
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("v1"));
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_keys = ON")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE schema_migrations (version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                                          "applied_at TEXT NOT NULL)")));
            for (const QString &statement : Migrator::migrations().first().statements)
                QVERIFY2(q.exec(statement), qPrintable(q.lastError().text()));
            const QStringList rows = {
                "INSERT INTO schema_migrations VALUES (1, 'initial_schema', '2026-10-01T00:00:00Z')",
                "INSERT INTO users (id, email, full_name, password_hash, role, created_at, updated_at) VALUES "
                "(7, 'ali@example.com', 'Ali Raza', 'x', 'customer', 'n', 'n')",
                "INSERT INTO categories (id, name, created_at) VALUES (1, 'Books', 'n')",
                "INSERT INTO products (id, sku, name, category_id, price_paisa, stock, created_at, updated_at) VALUES "
                "(3, 'BK-1', 'Book', 1, 100000, 4, 'n', 'n')",
                "INSERT INTO orders (id, order_number, user_id, status, subtotal_paisa, shipping_paisa, tax_paisa, "
                "total_paisa, payment_method, payment_status, ship_name, ship_phone, ship_line1, ship_city, "
                "checkout_token, created_at, updated_at) VALUES (11, 'SH-20261001-AAAAAA', 7, 'delivered', 100000, "
                "25000, 0, 125000, 'cod', 'paid', 'Ali', '0300 1234567', 'Street 1', 'Lahore', 'tok', "
                "'2026-10-01T09:00:00Z', 'n')",
                "INSERT INTO order_items (order_id, product_id, sku, product_name, unit_price_paisa, quantity, "
                "line_total_paisa) VALUES (11, 3, 'BK-1', 'Book', 100000, 1, 100000)",
                "INSERT INTO order_status_history (order_id, to_status, changed_at) VALUES (11, 'pending', 'n')",
            };
            for (const QString &row : rows)
                QVERIFY2(q.exec(row), qPrintable(q.lastError().text()));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("v1"));

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
        QCOMPARE(scalar(QStringLiteral("PRAGMA foreign_keys")).toInt(), 1);
        QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_key_check")));
        QVERIFY(!q.next());
        QVERIFY(q.exec(QStringLiteral("PRAGMA integrity_check")) && q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("ok"));

        // The old order, its lines and history survived with the same ids.
        QCOMPARE(scalar(QStringLiteral("SELECT order_number FROM orders WHERE id = 11")).toString(),
                 QStringLiteral("SH-20261001-AAAAAA"));
        QCOMPARE(scalar(QStringLiteral("SELECT total_paisa FROM orders WHERE id = 11")).toLongLong(), 125000);
        QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM order_items WHERE order_id = 11")).toInt(), 1);
        QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM order_status_history WHERE order_id = 11")).toInt(), 1);
        QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND tbl_name = 'orders' "
                                       "AND name LIKE 'idx_orders_%'")).toInt(), 3);
        QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE name = 'orders_new'")).toInt(), 0);

        // Order lines still point at the rebuilt orders table (foreign keys enforced).
        QVERIFY(!q.exec(QStringLiteral("INSERT INTO order_items (order_id, sku, product_name, unit_price_paisa, quantity, "
                                       "line_total_paisa) VALUES (999, 'X', 'X', 1, 1, 1)")));
        // The order lines are still immutable.
        QVERIFY(!q.exec(QStringLiteral("UPDATE order_items SET quantity = 5 WHERE order_id = 11")));

        // And the upgraded database takes khata orders.
        auto admin = ctx->auth().createFirstAdmin(QStringLiteral("Owner"), QStringLiteral("owner@test.pk"),
                                                  QStringLiteral("OwnerPass1"));
        QVERIFY(admin.isOk());
        QVERIFY(ctx->khata().setLimit(admin.value(), 7, Money::fromRupees(5000)).isOk());
        QCOMPARE(ctx->khata().account(admin.value(), 7).value().customerName, QStringLiteral("Ali Raza"));
    }

    // ------------------------------------------------------------------ sample data
    void sampleDataHasConsistentKhata()
    {
        // Clean back end: the sample categories clash with the fixture's.
        m_b = std::make_unique<test::Backend>();
        AppConfig cfg;
        cfg.databasePath = QStringLiteral(":memory:");
        cfg.imageDirectory = m_b->imageDir.path();
        cfg.passwordIterations = 1000;
        m_b->ctx = AppContext::create(cfg).takeValue();
        auto admin = m_b->ctx->auth().createFirstAdmin(QStringLiteral("Owner"), QStringLiteral("owner@test.pk"),
                                                       QStringLiteral("OwnerPass1"));
        QVERIFY(admin.isOk());
        const Status st = DemoSeeder::seed(*m_b->ctx, admin.value());
        QVERIFY2(st.isOk(), qPrintable(st.error()));

        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_accounts WHERE credit_limit_paisa > 0")),
                 DemoSeeder::sampleKhataCustomerCount());
        QVERIFY(count(QStringLiteral("SELECT COUNT(*) FROM orders WHERE payment_method = 'khata'")) >= 15);
        QVERIFY(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE kind = 'payment'")) >= 5);
        QVERIFY(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE kind = 'reversal'")) >= 1);
        // Delivered khata orders stay on khata; delivered COD orders are paid.
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders WHERE payment_method = 'khata' AND payment_status <> 'on_account'")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders WHERE payment_method = 'cod' AND status = 'delivered' "
                                      "AND payment_status <> 'paid'")), 0);
        // Every khata order has exactly one charge; every cancelled one exactly one reversal.
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders o WHERE payment_method = 'khata' AND "
                                      "(SELECT COUNT(*) FROM khata_entries e WHERE e.order_id = o.id AND kind = 'charge') <> 1")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders o WHERE payment_method = 'khata' AND status = 'cancelled' AND "
                                      "(SELECT COUNT(*) FROM khata_entries e WHERE e.order_id = o.id AND kind = 'reversal') <> 1")), 0);
        // Entries are dated like the orders they belong to (not "today").
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries e JOIN orders o ON o.id = e.order_id "
                                      "WHERE e.kind = 'charge' AND e.created_at <> o.created_at")), 0);
        QVERIFY(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE created_at < '%1'")
                          .arg(sql::toDb(QDateTime::currentDateTimeUtc().addDays(-7)))) > 5);

        // Running balances add up, nobody is over their limit, nobody is below zero.
        auto accounts = m_b->ctx->khata().accounts(admin.value()).value();
        QCOMPARE(accounts.size(), 50);
        Money owed;
        for (const KhataAccount &a : accounts) {
            verifyLedger(a.userId);
            QVERIFY2(!a.overLimit(), qPrintable(a.customerName));
            QVERIFY(!a.balance.isNegative());
            owed += a.balance;
        }
        QCOMPARE(owed, Money::fromRupees(87899));
        QCOMPARE(m_b->ctx->dashboard().stats(admin.value()).value().khataOutstanding, owed);
        QCOMPARE(m_b->ctx->dashboard().stats(admin.value()).value().khataDebtors, 7);

        // The account hinted on the sign-in screen has an open khata with room left.
        auto ayesha = m_b->ctx->auth().login(DemoSeeder::kCustomerEmail, DemoSeeder::kCustomerPassword);
        QVERIFY(ayesha.isOk());
        auto mine = m_b->ctx->khata().myAccount(ayesha.value()).value();
        QCOMPARE(mine.creditLimit, Money::fromRupees(30000));
        QVERIFY(mine.balance > Money());
        QVERIFY(mine.available() > Money::fromRupees(10000));
    }
};

QTEST_GUILESS_MAIN(TestKhata)
#include "tst_khata.moc"
