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

    // ------------------------------------------------------------------ wholesale tiers (PriceTier.h)
    void tierForPicksHighestReached()
    {
        const QVector<PriceTier> tiers = {tier(50, 1000), tier(12, 500)}; // any order
        QVERIFY(!pricetier::tierFor(tiers, 11).has_value());
        QCOMPARE(pricetier::tierFor(tiers, 12)->minQuantity, 12);
        QCOMPARE(pricetier::tierFor(tiers, 49)->minQuantity, 12);
        QCOMPARE(pricetier::tierFor(tiers, 50)->minQuantity, 50);
        QCOMPARE(pricetier::tierFor(tiers, 99)->minQuantity, 50);
        QVERIFY(!pricetier::tierFor({}, 99).has_value());
    }

    void nextTierForHints()
    {
        const QVector<PriceTier> tiers = {tier(50, 1000), tier(12, 500)};
        QCOMPARE(pricetier::nextTier(tiers, 1)->minQuantity, 12);
        QCOMPARE(pricetier::nextTier(tiers, 12)->minQuantity, 50);
        QVERIFY(!pricetier::nextTier(tiers, 50).has_value());
    }

    void tierPriceRoundsHalfUpAndNeverBelowOnePaisa()
    {
        // Rs 1,250 at 5% off = Rs 1,187.50 (the WHOLESALE.md example)
        QCOMPARE(pricetier::unitPrice(Money::fromRupees(1250), tier(12, 500)), Money::fromPaisa(118750));
        // 999 paisa at 5%: the discount 49.95 rounds half-up to 50 -> 949
        QCOMPARE(pricetier::unitPrice(Money::fromPaisa(999), tier(2, 500)), Money::fromPaisa(949));
        // 1 paisa at 90% off stays 1 paisa
        QCOMPARE(pricetier::unitPrice(Money::fromPaisa(1), tier(2, 9000)), Money::fromPaisa(1));
        QCOMPARE(pricetier::unitPriceFor(Money::fromRupees(100), {tier(10, 1000)}, 9), Money::fromRupees(100));
        QCOMPARE(pricetier::unitPriceFor(Money::fromRupees(100), {tier(10, 1000)}, 10), Money::fromRupees(90));
    }

    void validateRules()
    {
        QVERIFY(pricetier::validate({}).isEmpty());
        QVERIFY(pricetier::validate({tier(12, 500), tier(50, 1000)}).isEmpty());
        QVERIFY(pricetier::validate({tier(12, 500), tier(50, 500)}).isEmpty()); // equal discount is fine
        QVERIFY(!pricetier::validate({tier(12, 500), tier(50, 400)}).isEmpty()); // more units, smaller discount
        QVERIFY(!pricetier::validate({tier(12, 500), tier(12, 600)}).isEmpty()); // same start twice
        QVERIFY(!pricetier::validate({tier(1, 500)}).isEmpty());
        QVERIFY(!pricetier::validate({tier(100, 500)}).isEmpty());
        QVERIFY(!pricetier::validate({tier(12, 0)}).isEmpty());
        QVERIFY(!pricetier::validate({tier(12, 9001)}).isEmpty());
        QVERIFY(!pricetier::validate({tier(2, 100), tier(3, 200), tier(4, 300), tier(5, 400), tier(6, 500),
                                      tier(7, 600)})
                     .isEmpty()); // 6 tiers
    }

    void percentTextAndParsing()
    {
        QCOMPARE(pricetier::percentText(500), QStringLiteral("5"));
        QCOMPARE(pricetier::percentText(250), QStringLiteral("2.50"));
        QCOMPARE(pricetier::percentText(1), QStringLiteral("0.01"));
        QCOMPARE(pricetier::percentText(9000), QStringLiteral("90"));

        QCOMPARE(pricetier::parsePercent(QStringLiteral("5")).value_or(-1), 500);
        QCOMPARE(pricetier::parsePercent(QStringLiteral(" 2.5 ")).value_or(-1), 250);
        QCOMPARE(pricetier::parsePercent(QStringLiteral("12.75%")).value_or(-1), 1275);
        QCOMPARE(pricetier::parsePercent(QStringLiteral("0.01")).value_or(-1), 1);
        QVERIFY(!pricetier::parsePercent(QString()).has_value());
        QVERIFY(!pricetier::parsePercent(QStringLiteral("abc")).has_value());
        QVERIFY(!pricetier::parsePercent(QStringLiteral("-5")).has_value());
        QVERIFY(!pricetier::parsePercent(QStringLiteral("5.123")).has_value());
        QVERIFY(!pricetier::parsePercent(QStringLiteral("5,5")).has_value());
        // Round trip for every value an admin can save.
        for (int bp : {1, 50, 250, 500, 1275, 9000})
            QCOMPARE(pricetier::parsePercent(pricetier::percentText(bp)).value_or(-1), bp);
    }

    void applyTiersAtBoundaries()
    {
        QHash<qint64, QVector<PriceTier>> tiers;
        tiers[1] = {tier(12, 500), tier(50, 1000)};

        QVector<CartItem> lines = {item(125000, 11)};
        PricingService::applyTiers(lines, tiers);
        QCOMPARE(lines[0].unitPrice, Money::fromRupees(1250)); // one short of the tier
        QCOMPARE(lines[0].discountBp, 0);
        QCOMPARE(lines[0].unitsToNextTier(), 1);
        QCOMPARE(lines[0].nextTierUnitPrice, Money::fromPaisa(118750));

        lines = {item(125000, 12)};
        PricingService::applyTiers(lines, tiers);
        QCOMPARE(lines[0].unitPrice, Money::fromPaisa(118750));
        QCOMPARE(lines[0].basePrice, Money::fromRupees(1250));
        QCOMPARE(lines[0].discountBp, 500);
        QCOMPARE(lines[0].lineTotal(), Money::fromRupees(14250)); // 12 x Rs 1,187.50
        QCOMPARE(lines[0].lineSavings(), Money::fromRupees(750));
        QCOMPARE(lines[0].unitsToNextTier(), 38);

        lines = {item(125000, 50)};
        PricingService::applyTiers(lines, tiers);
        QCOMPARE(lines[0].unitPrice, Money::fromRupees(1125));
        QVERIFY(!lines[0].nextTier.has_value());
        QCOMPARE(lines[0].nextTierUnitPrice, Money());
    }

    void applyTiersIsPerProduct()
    {
        QHash<qint64, QVector<PriceTier>> tiers;
        tiers[1] = {tier(10, 1000)};
        CartItem other = item(50000, 20);
        other.productId = 2; // no tiers
        QVector<CartItem> lines = {item(10000, 10), other};
        PricingService::applyTiers(lines, tiers);
        QCOMPARE(lines[0].unitPrice, Money::fromRupees(90));
        QCOMPARE(lines[1].unitPrice, Money::fromRupees(500));
        QCOMPARE(lines[1].discountBp, 0);
        QVERIFY(!lines[1].nextTier.has_value());

        PricingService pricing;
        const PriceBreakdown b = pricing.price(lines);
        QCOMPARE(b.subtotal, Money::fromRupees(900 + 10000));
        QCOMPARE(b.savings, Money::fromRupees(100));
    }

    void freeDeliveryUsesDiscountedSubtotal()
    {
        QHash<qint64, QVector<PriceTier>> tiers;
        tiers[1] = {tier(10, 1000)};
        QVector<CartItem> lines = {item(52000, 10)}; // Rs 5,200 before, Rs 4,680 after 10% off
        PricingService::applyTiers(lines, tiers);
        PricingService pricing; // free from Rs 5,000
        const PriceBreakdown b = pricing.price(lines);
        QCOMPARE(b.subtotal, Money::fromRupees(4680));
        QCOMPARE(b.shipping, Money::fromRupees(250));
        QCOMPARE(b.total, Money::fromRupees(4930));
    }

private:
    static PriceTier tier(int minQuantity, int discountBp)
    {
        PriceTier t;
        t.minQuantity = minQuantity;
        t.discountBp = discountBp;
        return t;
    }
};

QTEST_GUILESS_MAIN(TestPricing)
#include "tst_pricing.moc"
