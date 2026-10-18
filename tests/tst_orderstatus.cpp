#include "domain/OrderStatus.h"

#include <QtTest>

using namespace shophub;
namespace os = shophub::orderstatus;

class TestOrderStatus : public QObject
{
    Q_OBJECT

private slots:
    void forwardChain()
    {
        QVERIFY(os::canTransition(OrderStatus::Pending, OrderStatus::Confirmed));
        QVERIFY(os::canTransition(OrderStatus::Confirmed, OrderStatus::Processing));
        QVERIFY(os::canTransition(OrderStatus::Processing, OrderStatus::Shipped));
        QVERIFY(os::canTransition(OrderStatus::Shipped, OrderStatus::Delivered));
    }

    void noSkippingOrGoingBack()
    {
        QVERIFY(!os::canTransition(OrderStatus::Pending, OrderStatus::Shipped));
        QVERIFY(!os::canTransition(OrderStatus::Shipped, OrderStatus::Confirmed));
        QVERIFY(!os::canTransition(OrderStatus::Delivered, OrderStatus::Pending));
        QVERIFY(!os::canTransition(OrderStatus::Cancelled, OrderStatus::Confirmed));
        QVERIFY(!os::canTransition(OrderStatus::Pending, OrderStatus::Pending));
    }

    void cancellationRules()
    {
        QVERIFY(os::customerCanCancel(OrderStatus::Pending));
        QVERIFY(os::customerCanCancel(OrderStatus::Confirmed));
        QVERIFY(!os::customerCanCancel(OrderStatus::Processing));
        QVERIFY(os::adminCanCancel(OrderStatus::Processing));
        QVERIFY(!os::adminCanCancel(OrderStatus::Shipped));
        QVERIFY(!os::adminCanCancel(OrderStatus::Delivered));
        QVERIFY(!os::adminCanCancel(OrderStatus::Cancelled));
        QVERIFY(!os::canTransition(OrderStatus::Shipped, OrderStatus::Cancelled));
    }

    void finalStates()
    {
        QVERIFY(os::isFinal(OrderStatus::Delivered));
        QVERIFY(os::isFinal(OrderStatus::Cancelled));
        QVERIFY(!os::nextForward(OrderStatus::Delivered).has_value());
        QVERIFY(!os::isFinal(OrderStatus::Shipped));
    }

    void dbRoundTrip()
    {
        for (OrderStatus s : os::all())
            QCOMPARE(os::fromDb(os::toDb(s)).value(), s);
        QVERIFY(!os::fromDb(QStringLiteral("bogus")).has_value());
        QCOMPARE(os::displayName(OrderStatus::Shipped), QStringLiteral("Shipped"));
    }
};

QTEST_GUILESS_MAIN(TestOrderStatus)
#include "tst_orderstatus.moc"
