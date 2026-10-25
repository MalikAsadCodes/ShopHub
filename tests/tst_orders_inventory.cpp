#include "TestSupport.h"

#include "services/DemoSeeder.h"

using namespace shophub;

class TestOrdersInventory : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
        m_product = m_b->addProduct(QStringLiteral("ORD-1"), 1000, 10, QStringLiteral("Mug"));
    }
    void cleanup() { m_b.reset(); }

    // ------------------------------------------------------------------ orders
    void customerCancelsPendingOrderAndStockReturns()
    {
        const Order o = placeOrder(3);
        QCOMPARE(m_b->stockOf(m_product), 7);
        QVERIFY(m_b->ctx->orders().cancel(m_b->customer, o.id, QString()).isOk());
        QCOMPARE(m_b->stockOf(m_product), 10);
        QCOMPARE(m_b->ctx->orders().details(m_b->customer, o.id).value().status, OrderStatus::Cancelled);
        QVERIFY2(!m_b->ctx->orders().cancel(m_b->customer, o.id, QString()).isOk(), "cannot cancel twice");
        QCOMPARE(m_b->stockOf(m_product), 10);
    }

    void customerCannotCancelOnceProcessing()
    {
        const Order o = placeOrder(1);
        QVERIFY(m_b->ctx->orders().advance(m_b->admin, o.id, OrderStatus::Confirmed).isOk());
        QVERIFY(m_b->ctx->orders().advance(m_b->admin, o.id, OrderStatus::Processing).isOk());
        QVERIFY(!m_b->ctx->orders().cancel(m_b->customer, o.id, QString()).isOk());
        QVERIFY(m_b->ctx->orders().cancel(m_b->admin, o.id, QStringLiteral("Supplier issue")).isOk());
        QCOMPARE(m_b->stockOf(m_product), 10);
    }

    void adminWalksOrderToDelivered()
    {
        const Order o = placeOrder(1);
        auto &orders = m_b->ctx->orders();
        QVERIFY(!orders.advance(m_b->admin, o.id, OrderStatus::Shipped).isOk()); // cannot skip
        for (OrderStatus s : {OrderStatus::Confirmed, OrderStatus::Processing, OrderStatus::Shipped, OrderStatus::Delivered})
            QVERIFY(orders.advance(m_b->admin, o.id, s).isOk());
        const Order done = orders.details(m_b->admin, o.id).value();
        QCOMPARE(done.status, OrderStatus::Delivered);
        QCOMPARE(done.paymentStatus, QStringLiteral("paid")); // COD collected on delivery
        QVERIFY(!orders.cancel(m_b->admin, o.id, QString()).isOk());
        QCOMPARE(orders.history(m_b->admin, o.id).value().size(), 5);
    }

    void customersCannotAdvanceOrSeeOthersOrders()
    {
        const Order o = placeOrder(1);
        QVERIFY(!m_b->ctx->orders().advance(m_b->customer, o.id, OrderStatus::Confirmed).isOk());
        QVERIFY(!m_b->ctx->orders().allOrders(m_b->customer, OrderQuery()).isOk());

        auto other = m_b->ctx->auth().registerCustomer(QStringLiteral("Other"), QStringLiteral("other@test.pk"),
                                                       QStringLiteral("OtherPass1"));
        QVERIFY(other.isOk());
        QVERIFY(!m_b->ctx->orders().details(other.value(), o.id).isOk());
        QVERIFY(!m_b->ctx->orders().cancel(other.value(), o.id, QString()).isOk());
        QVERIFY(m_b->ctx->orders().myOrders(other.value()).value().isEmpty());
        QCOMPARE(m_b->ctx->orders().myOrders(m_b->customer).value().size(), 1);
    }

    void adminOrderFilters()
    {
        const Order a = placeOrder(1);
        placeOrder(1);
        QVERIFY(m_b->ctx->orders().advance(m_b->admin, a.id, OrderStatus::Confirmed).isOk());
        OrderQuery q;
        q.status = OrderStatus::Pending;
        QCOMPARE(m_b->ctx->orders().allOrders(m_b->admin, q).value().size(), 1);
        OrderQuery search;
        search.search = a.orderNumber;
        QCOMPARE(m_b->ctx->orders().allOrders(m_b->admin, search).value().size(), 1);
    }

    // ------------------------------------------------------------------ inventory
    void stockAdjustmentRules()
    {
        auto &inv = m_b->ctx->inventory();
        QVERIFY(!inv.adjust(m_b->customer, m_product, 5, StockReason::Restock, QString()).isOk());
        QVERIFY(!inv.adjust(m_b->admin, m_product, 0, StockReason::Restock, QString()).isOk());
        QVERIFY(!inv.adjust(m_b->admin, m_product, -1, StockReason::Restock, QString()).isOk());
        QVERIFY(!inv.adjust(m_b->admin, m_product, 1, StockReason::Damaged, QString()).isOk());
        QVERIFY(!inv.adjust(m_b->admin, m_product, 2, StockReason::Adjustment, QString()).isOk()); // needs a note
        QVERIFY(!inv.adjust(m_b->admin, m_product, -11, StockReason::Damaged, QString()).isOk()); // below zero
        QVERIFY(!inv.adjust(m_b->admin, m_product, 5, StockReason::Sale, QString()).isOk());

        QCOMPARE(inv.adjust(m_b->admin, m_product, 15, StockReason::Restock, QString()).value(), 25);
        QCOMPARE(inv.adjust(m_b->admin, m_product, -5, StockReason::Damaged, QStringLiteral("Broken")).value(), 20);
        QCOMPARE(inv.adjust(m_b->admin, m_product, -1, StockReason::Adjustment, QStringLiteral("Count fix")).value(), 19);
        QCOMPARE(m_b->stockOf(m_product), 19);

        auto moves = inv.movements(m_b->admin, m_product).value();
        QCOMPARE(moves.size(), 4); // initial + 3 adjustments
        QCOMPARE(moves.first().stockAfter, 19);
        QCOMPARE(moves.first().userName, QStringLiteral("Admin User"));
    }

    // ------------------------------------------------------------------ dashboard
    void dashboardExcludesCancelledOrders()
    {
        const Order kept = placeOrder(2);      // Rs 2,000 + Rs 250 shipping
        const Order cancelled = placeOrder(1); // Rs 1,000 + Rs 250
        QVERIFY(m_b->ctx->orders().cancel(m_b->customer, cancelled.id, QString()).isOk());

        auto stats = m_b->ctx->dashboard().stats(m_b->admin);
        QVERIFY(stats.isOk());
        QCOMPARE(stats.value().orderCount, 1);
        QCOMPARE(stats.value().grossSales, kept.total);
        QCOMPARE(stats.value().averageOrderValue, kept.total);
        QCOMPARE(stats.value().pendingOrders, 1);
        QCOMPARE(stats.value().customerCount, 1);
        QCOMPARE(stats.value().last14Days.size(), 14);
        QCOMPARE(stats.value().last14Days.last().revenue, kept.total);
        QCOMPARE(stats.value().topProducts.first().units, 2);

        QVERIFY(!m_b->ctx->dashboard().stats(m_b->customer).isOk());
    }

    void emptyDashboard()
    {
        auto stats = m_b->ctx->dashboard().stats(m_b->admin).value();
        QCOMPARE(stats.orderCount, 0);
        QCOMPARE(stats.averageOrderValue, Money());
        QVERIFY(stats.topProducts.isEmpty());
    }

    // ------------------------------------------------------------------ demo data
    void demoDataLoadsThroughServices()
    {
        QVERIFY(!DemoSeeder::seed(*m_b->ctx, m_b->customer).isOk()); // customers cannot seed
        // Demo categories clash with the fixture's "Electronics", so start from a clean backend.
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
        QVERIFY(DemoSeeder::isLoaded(*m_b->ctx));
        QVERIFY(!DemoSeeder::seed(*m_b->ctx, admin.value()).isOk()); // only once

        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM products")).toInt(), 27);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM orders")).toInt(), DemoSeeder::sampleOrderCount());
        QVERIFY(DemoSeeder::sampleOrderCount() >= 60);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM users WHERE role = 'admin'")).toInt(), 1);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM users WHERE role = 'customer'")).toInt(), 50);
        // Orders are spread over about three weeks.
        QVERIFY(m_b->scalar(QStringLiteral("SELECT COUNT(DISTINCT substr(created_at, 1, 10)) FROM orders")).toInt() >= 15);
        // Low-stock items end with exactly the stock listed in the seeder.
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT stock FROM products WHERE sku = 'ELEC-1003'")).toInt(), 3);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT stock FROM products WHERE sku = 'ELEC-1001'")).toInt(), 40);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT stock FROM products WHERE sku = 'ELEC-1006'")).toInt(), 0);

        // Wholesale prices: some products have tiers, and the bulk orders paid the tier price.
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(DISTINCT product_id) FROM product_price_tiers")).toInt(),
                 DemoSeeder::sampleWholesaleProductCount());
        QVERIFY(DemoSeeder::sampleWholesaleProductCount() >= 3);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT stock FROM products WHERE sku = 'SPRT-5004'")).toInt(), 25);
        // 12 footballs at Rs 3,299 less 10% = Rs 2,969.10 each
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT unit_price_paisa FROM order_items WHERE sku = 'SPRT-5004' AND quantity = 12"))
                     .toLongLong(),
                 qint64(296910));
        // Regular sample orders stay below every tier, so they paid the normal price.
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM order_items oi JOIN products p ON p.id = oi.product_id "
                                            "WHERE oi.unit_price_paisa < p.price_paisa")).toInt(),
                 4); // the 4 lines of the 3 bulk orders

        // The hinted account shows orders in several states.
        auto login = m_b->ctx->auth().login(DemoSeeder::kCustomerEmail, DemoSeeder::kCustomerPassword);
        QVERIFY(login.isOk());
        QCOMPARE(m_b->ctx->orders().myOrders(login.value()).value().size(), 5);

        // Qist: plans, products and customers, and 5 qists in every state.
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM qist_plans")).toInt(), DemoSeeder::sampleQistPlanCount());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM products WHERE qist_allowed = 1")).toInt(),
                 DemoSeeder::sampleQistProductCount());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM khata_accounts WHERE qist_approved = 1")).toInt(),
                 DemoSeeder::sampleQistCustomerCount());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM orders WHERE payment_method = 'qist'")).toInt(),
                 DemoSeeder::sampleQistCount());
        QistQuery all;
        all.show = QistQuery::Show::All;
        const QVector<QistAccount> qists = m_b->ctx->qist().accounts(admin.value(), all).value();
        QCOMPARE(qists.size(), DemoSeeder::sampleQistCount());
        int completed = 0;
        int overdue = 0;
        int onTime = 0;
        int nothingPaid = 0;
        for (const QistAccount &q : qists) {
            Money sum;
            for (const QistInstallment &line : q.schedule)
                sum += line.amount;
            QCOMPARE(sum, q.qistPrice);
            const Order o = m_b->ctx->orders().details(admin.value(), q.orderId).value();
            if (o.status == OrderStatus::Delivered)
                QVERIFY2(q.downPaymentPaid(), qPrintable(q.customerName)); // delivered once the down payment was in
            if (q.state == QistState::Completed) {
                ++completed;
                QCOMPARE(o.paymentStatus, payment::kPaid);
            } else if (q.isOverdue()) {
                ++overdue;
            } else {
                ++onTime;
            }
            if (q.paid.isZero())
                ++nothingPaid;
        }
        QCOMPARE(completed, 1);
        QCOMPARE(overdue, 1);
        QCOMPARE(onTime, 3);
        QCOMPARE(nothingPaid, 1);
        QCOMPARE(qists.first().customerName, QStringLiteral("Mahnoor Abbasi")); // overdue first
        // The hinted account may buy on qist and has none yet, so it can be tried at checkout.
        QVERIFY(m_b->ctx->qist().myEligibility(login.value()).value().approved);
        QVERIFY(m_b->ctx->qist().myAccounts(login.value()).value().isEmpty());
        QCOMPARE(DemoSeeder::dataVersion(*m_b->ctx), DemoSeeder::kDataVersion);
        QVERIFY(!DemoSeeder::needsUpgrade(*m_b->ctx));

        // Every sample customer signs in with first name + "123".
        const auto accounts = DemoSeeder::sampleCustomers();
        QCOMPARE(accounts.size(), 50);
        QSet<QString> emails;
        for (const auto &a : accounts) {
            QCOMPARE(a.password, a.fullName.section(QLatin1Char(' '), 0, 0) + QStringLiteral("123"));
            QVERIFY2(!a.fullName.contains(QStringLiteral("demo"), Qt::CaseInsensitive), qPrintable(a.fullName));
            emails.insert(a.email);
            auto s = m_b->ctx->auth().login(a.email, a.password);
            QVERIFY2(s.isOk(), qPrintable(a.email + QStringLiteral(": ") + s.error()));
            QCOMPARE(s.value().fullName(), a.fullName);
        }
        QCOMPARE(emails.size(), 50);
    }

    // Sample data from an older ShopHub gets what newer versions added, once, when
    // the admin signs in (DemoSeeder::upgrade). A real shop is never touched.
    void olderSampleDataIsBroughtUpToDate()
    {
        m_b = std::make_unique<test::Backend>();
        AppConfig cfg;
        cfg.databasePath = QStringLiteral(":memory:");
        cfg.imageDirectory = m_b->imageDir.path();
        cfg.passwordIterations = 1000;
        m_b->ctx = AppContext::create(cfg).takeValue();
        AppContext &ctx = *m_b->ctx;
        auto admin = ctx.auth().createFirstAdmin(QStringLiteral("Owner"), QStringLiteral("owner@test.pk"),
                                                 QStringLiteral("OwnerPass1"));
        QVERIFY(admin.isOk());
        auto count = [this](const char *sql) { return m_b->scalar(QString::fromLatin1(sql)).toInt(); };

        // A real shop: no sample data, nothing to do.
        QCOMPARE(DemoSeeder::dataVersion(ctx), 0);
        QVERIFY(!DemoSeeder::needsUpgrade(ctx));
        QVERIFY(DemoSeeder::upgrade(ctx, admin.value()).isOk());
        QCOMPARE(count("SELECT COUNT(*) FROM products"), 0);

        // Sample data as ShopHub v2 milestone 1 left it.
        const Status seeded = DemoSeeder::seed(ctx, admin.value(), 1);
        QVERIFY2(seeded.isOk(), qPrintable(seeded.error()));
        QCOMPARE(DemoSeeder::dataVersion(ctx), 1);
        QVERIFY(DemoSeeder::needsUpgrade(ctx));
        QCOMPARE(count("SELECT COUNT(*) FROM product_price_tiers"), 0);
        QCOMPARE(count("SELECT COUNT(*) FROM qist_plans"), 0);
        const int regularOrders = count("SELECT COUNT(*) FROM orders");
        QCOMPARE(regularOrders, DemoSeeder::sampleOrderCount() - 3 - DemoSeeder::sampleQistCount());

        // Owais is filling his cart: his sample bulk order is skipped and the cart is left alone.
        auto owais = ctx.auth().login(QStringLiteral("owais.chaudhry@example.com"), QStringLiteral("Owais123"));
        QVERIFY(owais.isOk());
        ProductFilter f;
        f.text = QStringLiteral("Yoga Mat");
        QVERIFY(ctx.cart().add(owais.value(), ctx.catalog().search(owais.value(), f).value().items.first().id, 1).isOk());

        QVERIFY(!DemoSeeder::upgrade(ctx, owais.value()).isOk()); // admin only
        const Status upgraded = DemoSeeder::upgrade(ctx, admin.value());
        QVERIFY2(upgraded.isOk(), qPrintable(upgraded.error()));
        QCOMPARE(DemoSeeder::dataVersion(ctx), DemoSeeder::kDataVersion);
        QVERIFY(!DemoSeeder::needsUpgrade(ctx));

        // Version 2: wholesale prices and the bulk orders (minus Owais's).
        QCOMPARE(count("SELECT COUNT(DISTINCT product_id) FROM product_price_tiers"), DemoSeeder::sampleWholesaleProductCount());
        const int tierRows = count("SELECT COUNT(*) FROM product_price_tiers");
        QCOMPARE(count("SELECT COUNT(*) FROM order_items WHERE sku = 'BOOK-4002' AND quantity = 10"), 1);
        QCOMPARE(count("SELECT COUNT(*) FROM order_items WHERE sku = 'SPRT-5004' AND quantity = 12"), 0);
        QCOMPARE(ctx.cart().itemCount(owais.value()).value(), 1);
        // Version 3: qist.
        QCOMPARE(count("SELECT COUNT(*) FROM qist_plans"), DemoSeeder::sampleQistPlanCount());
        QCOMPARE(count("SELECT COUNT(*) FROM products WHERE qist_allowed = 1"), DemoSeeder::sampleQistProductCount());
        QCOMPARE(count("SELECT COUNT(*) FROM khata_accounts WHERE qist_approved = 1"), DemoSeeder::sampleQistCustomerCount());
        QCOMPARE(count("SELECT COUNT(*) FROM qist_accounts"), DemoSeeder::sampleQistCount());
        QistQuery overdueOnly;
        overdueOnly.show = QistQuery::Show::Overdue;
        QCOMPARE(ctx.qist().accounts(admin.value(), overdueOnly).value().size(), 1);
        const int orders = count("SELECT COUNT(*) FROM orders");
        QCOMPARE(orders, DemoSeeder::sampleOrderCount() - 1);
        // Every ledger still adds up.
        QCOMPARE(count("SELECT COUNT(*) FROM users u WHERE u.role = 'customer' AND "
                       "COALESCE((SELECT SUM(amount_paisa) FROM khata_entries e WHERE e.user_id = u.id), 0) <> "
                       "COALESCE((SELECT balance_after_paisa FROM khata_entries e WHERE e.user_id = u.id "
                       "ORDER BY e.id DESC LIMIT 1), 0)"),
                 0);

        // Only once: even with the version set back, nothing is added twice.
        QVERIFY(DemoSeeder::upgrade(ctx, admin.value()).isOk());
        QVERIFY(ctx.settings().set(QStringLiteral("demo_data_version"), QStringLiteral("1")).isOk());
        QVERIFY(DemoSeeder::upgrade(ctx, admin.value()).isOk());
        QCOMPARE(count("SELECT COUNT(*) FROM orders"), orders);
        QCOMPARE(count("SELECT COUNT(*) FROM product_price_tiers"), tierRows);
        QCOMPARE(count("SELECT COUNT(*) FROM qist_plans"), DemoSeeder::sampleQistPlanCount());
        QCOMPARE(count("SELECT COUNT(*) FROM qist_accounts"), DemoSeeder::sampleQistCount());
    }

    // docs/customer_accounts.csv is the list people read; it must match the app exactly.
    void accountsFileMatchesSampleData()
    {
        QFile file(QStringLiteral(SHOPHUB_SOURCE_DIR "/docs/customer_accounts.csv"));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.fileName()));
        QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.takeFirst(), QStringLiteral("No,Full name,Email,Password,City"));

        const auto accounts = DemoSeeder::sampleCustomers();
        QCOMPARE(lines.size(), accounts.size());
        for (int i = 0; i < accounts.size(); ++i) {
            const auto &a = accounts[i];
            QCOMPARE(lines[i], QStringLiteral("%1,%2,%3,%4,%5").arg(i + 1).arg(a.fullName, a.email, a.password, a.city));
        }

        QFile md(QStringLiteral(SHOPHUB_SOURCE_DIR "/docs/CUSTOMER_ACCOUNTS.md"));
        QVERIFY(md.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(md.readAll());
        for (const auto &a : accounts) {
            QVERIFY2(text.contains(QStringLiteral("`%1` | `%2`").arg(a.email, a.password)), qPrintable(a.email));
        }
    }

private:
    Order placeOrder(int quantity)
    {
        const Status added = m_b->ctx->cart().add(m_b->customer, m_product, quantity);
        if (!added)
            qWarning() << added.error();
        auto o = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        if (!o)
            qWarning() << o.error();
        return o ? o.value() : Order();
    }

    std::unique_ptr<test::Backend> m_b;
    qint64 m_product = 0;
};

QTEST_GUILESS_MAIN(TestOrdersInventory)
#include "tst_orders_inventory.moc"
