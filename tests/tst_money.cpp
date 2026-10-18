#include "common/Money.h"

#include <QtTest>

using shophub::Money;

class TestMoney : public QObject
{
    Q_OBJECT

private slots:
    void parse_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<qint64>("paisa");

        QTest::newRow("whole") << "1500" << true << qint64(150000);
        QTest::newRow("one decimal") << "1500.5" << true << qint64(150050);
        QTest::newRow("two decimals") << "1500.05" << true << qint64(150005);
        QTest::newRow("commas") << "1,234,567.89" << true << qint64(123456789);
        QTest::newRow("rs prefix") << "Rs 99" << true << qint64(9900);
        QTest::newRow("spaces") << "  42  " << true << qint64(4200);
        QTest::newRow("zero") << "0" << true << qint64(0);
        QTest::newRow("three decimals") << "1.005" << false << qint64(0);
        QTest::newRow("negative") << "-5" << false << qint64(0);
        QTest::newRow("letters") << "12abc" << false << qint64(0);
        QTest::newRow("empty") << "" << false << qint64(0);
        QTest::newRow("dot only") << "." << false << qint64(0);
        QTest::newRow("too large") << "99999999999999" << false << qint64(0);
    }

    void parse()
    {
        QFETCH(QString, input);
        QFETCH(bool, valid);
        QFETCH(qint64, paisa);
        const auto m = Money::parse(input);
        QCOMPARE(m.has_value(), valid);
        if (valid)
            QCOMPARE(m->paisa(), paisa);
    }

    void formatting()
    {
        QCOMPARE(Money::fromPaisa(0).toString(), QStringLiteral("Rs 0.00"));
        QCOMPARE(Money::fromPaisa(5).toString(), QStringLiteral("Rs 0.05"));
        QCOMPARE(Money::fromPaisa(123456789).toString(), QStringLiteral("Rs 1,234,567.89"));
        QCOMPARE(Money::fromRupees(1000).toString(), QStringLiteral("Rs 1,000.00"));
        QCOMPARE(Money::fromRupees(999).toString(), QStringLiteral("Rs 999.00"));
        QCOMPARE(Money::fromPaisa(-25050).toString(), QStringLiteral("-Rs 250.50"));
        QCOMPARE(Money::fromPaisa(150005).toPlainString(), QStringLiteral("1500.05"));
    }

    void roundTrip()
    {
        const Money m = Money::fromPaisa(98765);
        QCOMPARE(Money::parse(m.toPlainString())->paisa(), m.paisa());
        QCOMPARE(Money::parse(m.toString())->paisa(), m.paisa());
    }

    void arithmetic()
    {
        const Money a = Money::fromRupees(10);
        const Money b = Money::fromPaisa(250);
        QCOMPARE((a + b).paisa(), qint64(1250));
        QCOMPARE((a - b).paisa(), qint64(750));
        QCOMPARE((b * 3).paisa(), qint64(750));
        QVERIFY(b < a);
    }

    void percentRounding()
    {
        // 17% of Rs 1.00 = 17 paisa exactly
        QCOMPARE(Money::fromRupees(1).percentOf(1700).paisa(), qint64(17));
        // 5% of 10 paisa = 0.5 -> rounds half up to 1
        QCOMPARE(Money::fromPaisa(10).percentOf(500).paisa(), qint64(1));
        // 5% of 9 paisa = 0.45 -> 0
        QCOMPARE(Money::fromPaisa(9).percentOf(500).paisa(), qint64(0));
        QCOMPARE(Money::fromRupees(5000).percentOf(0).paisa(), qint64(0));
        QCOMPARE(Money::fromRupees(5000).percentOf(10000).paisa(), qint64(500000));
    }
};

QTEST_GUILESS_MAIN(TestMoney)
#include "tst_money.moc"
