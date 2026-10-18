#include "common/Validation.h"

#include <QtTest>

using namespace shophub;

class TestValidation : public QObject
{
    Q_OBJECT

private slots:
    void emails()
    {
        QVERIFY(validation::isValidEmail(QStringLiteral("ali@example.com")));
        QVERIFY(validation::isValidEmail(QStringLiteral("first.last+tag@mail.co.pk")));
        QVERIFY(!validation::isValidEmail(QStringLiteral("no-at-sign.com")));
        QVERIFY(!validation::isValidEmail(QStringLiteral("a@b")));
        QVERIFY(!validation::isValidEmail(QStringLiteral("spaces in@mail.com")));
        QVERIFY(!validation::isValidEmail(QString()));
        QCOMPARE(validation::normalizeEmail(QStringLiteral("  Ali@Example.COM ")), QStringLiteral("ali@example.com"));
    }

    void passwords()
    {
        QVERIFY(validation::checkPassword(QStringLiteral("abcdefg1")).isOk());
        QVERIFY(!validation::checkPassword(QStringLiteral("abc1")).isOk());       // too short
        QVERIFY(!validation::checkPassword(QStringLiteral("abcdefgh")).isOk());   // no digit
        QVERIFY(!validation::checkPassword(QStringLiteral("12345678")).isOk());   // no letter
        QVERIFY(!validation::checkPassword(QString(129, QLatin1Char('a')) + QLatin1Char('1')).isOk());
    }

    void phones()
    {
        QVERIFY(validation::isValidPhone(QStringLiteral("0300 1234567")));
        QVERIFY(validation::isValidPhone(QStringLiteral("+92-300-1234567")));
        QVERIFY(!validation::isValidPhone(QStringLiteral("12345")));
        QVERIFY(!validation::isValidPhone(QStringLiteral("0300abc4567")));
    }

    void skus()
    {
        QVERIFY(validation::isValidSku(QStringLiteral("ELEC-1001")));
        QVERIFY(validation::isValidSku(QStringLiteral("AB1")));
        QVERIFY(!validation::isValidSku(QStringLiteral("ab-1")));   // lower case
        QVERIFY(!validation::isValidSku(QStringLiteral("-ABC")));   // leading dash
        QVERIFY(!validation::isValidSku(QStringLiteral("AB")));     // too short
        QVERIFY(!validation::isValidSku(QStringLiteral("AB C")));
    }

    void required()
    {
        QVERIFY(!validation::checkRequired(QStringLiteral("   "), QStringLiteral("Name"), 10).isOk());
        QVERIFY(!validation::checkRequired(QStringLiteral("12345678901"), QStringLiteral("Name"), 10).isOk());
        QVERIFY(validation::checkRequired(QStringLiteral("Ali"), QStringLiteral("Name"), 10).isOk());
    }

    void likeEscaping()
    {
        QCOMPARE(validation::escapeLike(QStringLiteral("50%_off\\")), QStringLiteral("50\\%\\_off\\\\"));
    }
};

QTEST_GUILESS_MAIN(TestValidation)
#include "tst_validation.moc"
