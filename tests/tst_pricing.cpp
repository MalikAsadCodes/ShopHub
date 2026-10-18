#include "services/CartService.h"
#include "services/PricingService.h"

#include <QtTest>

using namespace shophub;

namespace {
CartItem item(qint64 pricePaisa, int qty, int stock = 100, bool active = true)
{
    CartItem i;
    i.productId = 1;
    i.name = QStringLiteral("Item");
    i.unitPrice = Money::fromPaisa(pricePaisa);
    i.quantity = qty;
    i.availableStock = stock;
    i.productActive = active;
    return i;
}
} // namespace

class TestPricing : public QObject
{
    Q_OBJECT

private slots:
    void emptyCartIsFree()
    {
        PricingService pricing;
        const PriceBreakdown b = pricing.price({});
        QCOMPARE(b.total.paisa(), qint64(0));
        QCOMPARE(b.shipping.paisa(), qint64(0));
        QCOMPARE(b.itemCount, 0);
    }

    void flatShippingBelowThreshold()
    {
        PricingService pricing; // Rs 250 flat, free from Rs 5,000
        const PriceBreakdown b = pricing.price({item(100000, 2)}); // Rs 2,000
        QCOMPARE(b.subtotal, Money::fromRupees(2000));
        QCOMPARE(b.shipping, Money::fromRupees(250));
        QCOMPARE(b.total, Money::fromRupees(2250));
        QCOMPARE(b.itemCount, 2);
    }

    void freeShippingAtThreshold()
    {
        PricingService pricing;
        QCOMPARE(pricing.price({item(500000, 1)}).shipping, Money());              // exactly Rs 5,000
        QCOMPARE(pricing.price({item(499999, 1)}).shipping, Money::fromRupees(250)); // 1 paisa short
    }

    void taxIsOnGoodsOnlyAndRounded()
    {
        PricingRules rules;
        rules.taxRateBp = 1700; // 17%
        PricingService pricing(rules);
        // Rs 999.99 * 17% = 169.9983 -> Rs 170.00
        const PriceBreakdown b = pricing.price({item(99999, 1)});
        QCOMPARE(b.tax.paisa(), qint64(17000));
        QCOMPARE(b.total, b.subtotal + b.shipping + b.tax);
    }

    void multipleLines()
    {
        PricingService pricing;
        const PriceBreakdown b = pricing.price({item(150050, 3), item(9999, 2)});
        QCOMPARE(b.subtotal.paisa(), qint64(150050 * 3 + 9999 * 2));
        QCOMPARE(b.itemCount, 5);
    }

    void cartProblemsDetected()
    {
        QVERIFY(CartService::problems({item(100, 1, 5)}).isEmpty());
        QCOMPARE(CartService::problems({item(100, 6, 5)}).size(), 1);      // more than stock
        QCOMPARE(CartService::problems({item(100, 1, 0)}).size(), 1);      // out of stock
        QCOMPARE(CartService::problems({item(100, 1, 5, false)}).size(), 1); // deactivated
    }
};

QTEST_GUILESS_MAIN(TestPricing)
#include "tst_pricing.moc"
