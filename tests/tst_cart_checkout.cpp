#include "TestSupport.h"

using namespace shophub;

class TestCartCheckout : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
        m_kettle = m_b->addProduct(QStringLiteral("KET-1"), 2000, 5, QStringLiteral("Kettle"));
        m_lamp = m_b->addProduct(QStringLiteral("LMP-1"), 1500, 3, QStringLiteral("Lamp"));
    }
    void cleanup() { m_b.reset(); }

    // ------------------------------------------------------------------ cart
    void cartPersistsPerUser()
    {
        auto &cart = m_b->ctx->cart();
        QVERIFY(cart.add(m_b->customer, m_kettle, 2).isOk());
        QVERIFY(cart.add(m_b->customer, m_kettle, 1).isOk()); // adds up
        auto items = cart.items(m_b->customer);
        QCOMPARE(items.value().size(), 1);
        QCOMPARE(items.value().first().quantity, 3);
        QCOMPARE(cart.itemCount(m_b->customer).value(), 3);
    }

    void cannotExceedStock()
    {
        auto &cart = m_b->ctx->cart();
        QVERIFY(!cart.add(m_b->customer, m_lamp, 4).isOk());
        QVERIFY(cart.add(m_b->customer, m_lamp, 3).isOk());
        QVERIFY(!cart.add(m_b->customer, m_lamp, 1).isOk());
        QVERIFY(!cart.setQuantity(m_b->customer, m_lamp, 4).isOk());
        QVERIFY(!cart.add(m_b->customer, m_lamp, 0).isOk());
    }

    void setQuantityZeroRemoves()
    {
        auto &cart = m_b->ctx->cart();
        QVERIFY(cart.add(m_b->customer, m_kettle, 1).isOk());
        QVERIFY(cart.setQuantity(m_b->customer, m_kettle, 0).isOk());
        QVERIFY(cart.items(m_b->customer).value().isEmpty());
    }

    void adminHasNoCart()
    {
        QVERIFY(!m_b->ctx->cart().add(m_b->admin, m_kettle, 1).isOk());
    }

    void inactiveProductCannotBeAdded()
    {
        QVERIFY(m_b->ctx->catalog().setProductActive(m_b->admin, m_kettle, false).isOk());
        QVERIFY(!m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk());
    }

    void summaryUsesPricingRules()
    {
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk()); // Rs 2,000
        auto s = m_b->ctx->cart().summary(m_b->customer).value();
        QCOMPARE(s.subtotal, Money::fromRupees(2000));
        QCOMPARE(s.shipping, Money::fromRupees(250));
        QCOMPARE(s.total, Money::fromRupees(2250));
    }

    // ------------------------------------------------------------------ checkout
    void successfulCheckout()
    {
        auto &cart = m_b->ctx->cart();
        QVERIFY(cart.add(m_b->customer, m_kettle, 2).isOk()); // 4,000
        QVERIFY(cart.add(m_b->customer, m_lamp, 1).isOk());   // 1,500

        auto result = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY2(result.isOk(), qPrintable(result.error()));
        const Order o = result.value();

        QVERIFY(o.orderNumber.startsWith(QStringLiteral("SH-")));
        QCOMPARE(o.status, OrderStatus::Pending);
        QCOMPARE(o.subtotal, Money::fromRupees(5500));
        QCOMPARE(o.shipping, Money());                // over the free-shipping threshold
        QCOMPARE(o.total, Money::fromRupees(5500));
        QCOMPARE(o.items.size(), 2);
        QCOMPARE(o.paymentMethod, QStringLiteral("cod"));
        QCOMPARE(o.paymentStatus, QStringLiteral("unpaid"));

        QCOMPARE(m_b->stockOf(m_kettle), 3);
        QCOMPARE(m_b->stockOf(m_lamp), 2);
        QVERIFY(cart.items(m_b->customer).value().isEmpty());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM inventory_movements WHERE reason = 'sale'")).toInt(), 2);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM order_status_history")).toInt(), 1);

        auto saved = m_b->ctx->checkout().savedAddress(m_b->customer);
        QVERIFY(saved.value().has_value());
        QCOMPARE(saved.value()->city, QStringLiteral("Islamabad"));
    }

    void priceSnapshotSurvivesPriceChange()
    {
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk());
        auto o = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY(o.isOk());

        auto product = m_b->ctx->catalog().product(m_b->admin, m_kettle).value();
        ProductInput in;
        in.sku = product.sku;
        in.name = QStringLiteral("Renamed Kettle");
        in.categoryId = product.categoryId;
        in.price = Money::fromRupees(9999);
        QVERIFY(m_b->ctx->catalog().updateProduct(m_b->admin, m_kettle, in).isOk());

        auto reloaded = m_b->ctx->orders().details(m_b->customer, o.value().id).value();
        QCOMPARE(reloaded.items.first().unitPrice, Money::fromRupees(2000));
        QCOMPARE(reloaded.items.first().productName, QStringLiteral("Kettle"));
    }

    void duplicateSubmissionCreatesOneOrder()
    {
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk());
        const CheckoutRequest req = m_b->checkoutRequest();
        auto first = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        auto second = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(first.isOk());
        QVERIFY(second.isOk());
        QCOMPARE(second.value().id, first.value().id);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM orders")).toInt(), 1);
        QCOMPARE(m_b->stockOf(m_kettle), 4);
    }

    void emptyCartRejected()
    {
        QVERIFY(!m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest()).isOk());
    }

    void stockDroppedAfterAddingToCart()
    {
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_lamp, 3).isOk());
        // Admin writes off 2 lamps while they sit in the cart.
        QVERIFY(m_b->ctx->inventory().adjust(m_b->admin, m_lamp, -2, StockReason::Damaged, QString()).isOk());
        auto result = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY(!result.isOk());
        QVERIFY(result.error().contains(QStringLiteral("Lamp")));
        QCOMPARE(m_b->stockOf(m_lamp), 1);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM orders")).toInt(), 0);
    }

    void invalidShippingRejected()
    {
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk());
        CheckoutRequest req = m_b->checkoutRequest();
        req.shipping.phone = QStringLiteral("123");
        QVERIFY(!m_b->ctx->checkout().placeOrder(m_b->customer, req).isOk());
        req = m_b->checkoutRequest();
        req.shipping.city.clear();
        QVERIFY(!m_b->ctx->checkout().placeOrder(m_b->customer, req).isOk());
        req = m_b->checkoutRequest();
        req.paymentMethod = QStringLiteral("card");
        QVERIFY(!m_b->ctx->checkout().placeOrder(m_b->customer, req).isOk());
    }

    void adminCannotCheckout()
    {
        QVERIFY(!m_b->ctx->checkout().placeOrder(m_b->admin, m_b->checkoutRequest()).isOk());
    }

    // A failure at any point inside the transaction must leave no trace.
    void failedCheckoutRollsBackEverything_data()
    {
        QTest::addColumn<QString>("stage");
        QTest::newRow("after order insert") << QStringLiteral("after_order_insert");
        QTest::newRow("after stock update") << QStringLiteral("after_stock_update");
        QTest::newRow("right before commit") << QStringLiteral("before_commit");
    }

    void failedCheckoutRollsBackEverything()
    {
        QFETCH(QString, stage);
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 2).isOk());
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_lamp, 1).isOk());

        m_b->ctx->checkout().setFaultInjectorForTesting([stage](const QString &s) {
            return s == stage ? Status::fail(QStringLiteral("injected failure")) : Status::ok();
        });
        auto result = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY(!result.isOk());

        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM orders")).toInt(), 0);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM order_items")).toInt(), 0);
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM inventory_movements WHERE reason = 'sale'")).toInt(), 0);
        QCOMPARE(m_b->stockOf(m_kettle), 5);
        QCOMPARE(m_b->stockOf(m_lamp), 3);
        QCOMPARE(m_b->ctx->cart().items(m_b->customer).value().size(), 2);

        // And the next attempt works normally.
        m_b->ctx->checkout().setFaultInjectorForTesting(nullptr);
        QVERIFY(m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest()).isOk());
        QCOMPARE(m_b->stockOf(m_kettle), 3);
    }

    // ------------------------------------------------------------------ wholesale prices
    void cartShowsTierPriceFromTheRightQuantity()
    {
        const qint64 oil = wholesaleProduct(); // Rs 1,250; 12+: 5%, 50+: 10%
        auto &cart = m_b->ctx->cart();

        QVERIFY(cart.add(m_b->customer, oil, 11).isOk());
        CartItem line = cart.items(m_b->customer).value().first();
        QCOMPARE(line.unitPrice, Money::fromRupees(1250));
        QCOMPARE(line.unitsToNextTier(), 1);
        QCOMPARE(line.nextTierUnitPrice, Money::fromPaisa(118750));

        QVERIFY(cart.add(m_b->customer, oil, 1).isOk()); // now 12
        line = cart.items(m_b->customer).value().first();
        QCOMPARE(line.basePrice, Money::fromRupees(1250));
        QCOMPARE(line.unitPrice, Money::fromPaisa(118750));
        QCOMPARE(line.discountBp, 500);

        auto s = cart.summary(m_b->customer).value();
        QCOMPARE(s.subtotal, Money::fromRupees(14250));
        QCOMPARE(s.savings, Money::fromRupees(750));
        QCOMPARE(s.shipping, Money()); // free delivery on the discounted subtotal

        QVERIFY(cart.setQuantity(m_b->customer, oil, 50).isOk());
        QCOMPARE(cart.items(m_b->customer).value().first().unitPrice, Money::fromRupees(1125));
        QVERIFY(cart.setQuantity(m_b->customer, oil, 11).isOk()); // back below every tier
        QCOMPARE(cart.items(m_b->customer).value().first().unitPrice, Money::fromRupees(1250));
    }

    void checkoutStoresTheTierPrice()
    {
        const qint64 oil = wholesaleProduct();
        QVERIFY(m_b->ctx->cart().add(m_b->customer, oil, 12).isOk());
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk()); // no tiers: Rs 2,000

        auto result = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY2(result.isOk(), qPrintable(result.error()));
        const Order o = result.value();
        QCOMPARE(o.subtotal, Money::fromRupees(14250 + 2000));
        QCOMPARE(o.total, o.subtotal + o.shipping + o.tax);

        bool sawOil = false;
        for (const OrderItem &line : o.items) {
            if (line.sku == QStringLiteral("OIL-5L")) {
                sawOil = true;
                QCOMPARE(line.unitPrice, Money::fromPaisa(118750));
                QCOMPARE(line.quantity, 12);
                QCOMPARE(line.lineTotal, Money::fromRupees(14250));
            } else {
                QCOMPARE(line.unitPrice, Money::fromRupees(2000));
            }
        }
        QVERIFY(sawOil);
    }

    void oldOrdersKeepTheirTierPrice()
    {
        const qint64 oil = wholesaleProduct();
        QVERIFY(m_b->ctx->cart().add(m_b->customer, oil, 12).isOk());
        QVERIFY(m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest()).isOk());

        // The shop removes the tiers afterwards; the order still shows what was paid.
        QVERIFY(m_b->ctx->catalog().setPriceTiers(m_b->admin, oil, {}).isOk());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT unit_price_paisa FROM order_items WHERE sku = 'OIL-5L'")).toLongLong(),
                 qint64(118750));
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT subtotal_paisa FROM orders")).toLongLong(), qint64(1425000));
    }

    void checkoutUsesTiersSavedAfterTheCartWasFilled()
    {
        // The price is worked out again inside the checkout transaction, never taken from the screen.
        const qint64 oil = wholesaleProduct();
        QVERIFY(m_b->ctx->cart().add(m_b->customer, oil, 12).isOk());
        PriceTier better;
        better.minQuantity = 10;
        better.discountBp = 2000;
        QVERIFY(m_b->ctx->catalog().setPriceTiers(m_b->admin, oil, {better}).isOk());

        auto result = m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest());
        QVERIFY2(result.isOk(), qPrintable(result.error()));
        QCOMPARE(result.value().subtotal, Money::fromRupees(12000)); // 12 x Rs 1,000
    }

private:
    // A 5-litre cooking oil sold by the carton: Rs 1,250, 12 or more 5% off, 50 or more 10% off.
    qint64 wholesaleProduct()
    {
        const qint64 id = m_b->addProduct(QStringLiteral("OIL-5L"), 1250, 99, QStringLiteral("Cooking oil 5L"));
        PriceTier dozen;
        dozen.minQuantity = 12;
        dozen.discountBp = 500;
        PriceTier carton;
        carton.minQuantity = 50;
        carton.discountBp = 1000;
        const Status st = m_b->ctx->catalog().setPriceTiers(m_b->admin, id, {dozen, carton});
        if (!st)
            qWarning() << "setPriceTiers failed:" << st.error();
        return id;
    }

    std::unique_ptr<test::Backend> m_b;
    qint64 m_kettle = 0;
    qint64 m_lamp = 0;
};

QTEST_GUILESS_MAIN(TestCartCheckout)
#include "tst_cart_checkout.moc"
