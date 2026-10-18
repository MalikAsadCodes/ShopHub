#include "TestSupport.h"

using namespace shophub;

class TestAuth : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
    }
    void cleanup() { m_b.reset(); }

    void loginSucceedsWithCorrectPassword()
    {
        auto s = m_b->ctx->auth().login(QStringLiteral("cust@test.pk"), QStringLiteral("CustPass1"));
        QVERIFY2(s.isOk(), qPrintable(s.error()));
        QVERIFY(s.value().isCustomer());
        QCOMPARE(s.value().fullName(), QStringLiteral("Test Customer"));
    }

    void emailIsCaseInsensitive()
    {
        QVERIFY(m_b->ctx->auth().login(QStringLiteral("  CUST@Test.PK "), QStringLiteral("CustPass1")).isOk());
    }

    void wrongPasswordAndUnknownEmailGiveSameMessage()
    {
        auto wrong = m_b->ctx->auth().login(QStringLiteral("cust@test.pk"), QStringLiteral("nope12345"));
        auto unknown = m_b->ctx->auth().login(QStringLiteral("ghost@test.pk"), QStringLiteral("nope12345"));
        QVERIFY(!wrong.isOk());
        QVERIFY(!unknown.isOk());
        QCOMPARE(wrong.error(), unknown.error());
    }

    void passwordIsNeverStoredInPlainText()
    {
        const QString stored = m_b->scalar(QStringLiteral("SELECT password_hash FROM users WHERE email = 'cust@test.pk'")).toString();
        QVERIFY(stored.startsWith(QStringLiteral("pbkdf2_sha512$")));
        QVERIFY(!stored.contains(QStringLiteral("CustPass1")));
    }

    void sameDifferentSaltsForSamePassword()
    {
        PasswordHasher h(1000);
        const QString a = h.hash(QStringLiteral("Secret123"));
        const QString b = h.hash(QStringLiteral("Secret123"));
        QVERIFY(a != b);
        QVERIFY(h.verify(QStringLiteral("Secret123"), a));
        QVERIFY(h.verify(QStringLiteral("Secret123"), b));
        QVERIFY(!h.verify(QStringLiteral("secret123"), a));
        QVERIFY(!h.verify(QStringLiteral("Secret123"), QStringLiteral("garbage")));
        QVERIFY(PasswordHasher(2000).needsRehash(a));
        QVERIFY(!PasswordHasher(1000).needsRehash(a));
    }

    void duplicateEmailRejected()
    {
        auto again = m_b->ctx->auth().registerCustomer(QStringLiteral("Someone"), QStringLiteral("CUST@test.pk"),
                                                      QStringLiteral("Another1"));
        QVERIFY(!again.isOk());
        QVERIFY(again.error().contains(QStringLiteral("already exists")));
    }

    void registrationValidatesInput()
    {
        auto &auth = m_b->ctx->auth();
        QVERIFY(!auth.registerCustomer(QString(), QStringLiteral("a@b.pk"), QStringLiteral("Passw0rd")).isOk());
        QVERIFY(!auth.registerCustomer(QStringLiteral("A"), QStringLiteral("bad-email"), QStringLiteral("Passw0rd")).isOk());
        QVERIFY(!auth.registerCustomer(QStringLiteral("A"), QStringLiteral("a@b.pk"), QStringLiteral("short1")).isOk());
    }

    void lockoutAfterRepeatedFailures()
    {
        auto &auth = m_b->ctx->auth();
        for (int i = 0; i < AuthService::kMaxFailedLogins; ++i)
            QVERIFY(!auth.login(QStringLiteral("cust@test.pk"), QStringLiteral("wrong-pass1")).isOk());

        // Even the right password is refused while locked.
        auto locked = auth.login(QStringLiteral("cust@test.pk"), QStringLiteral("CustPass1"));
        QVERIFY(!locked.isOk());
        QVERIFY(locked.error().contains(QStringLiteral("Too many failed attempts")));

        // After the lock expires the right password works again.
        QSqlQuery q(QSqlDatabase::database(m_b->ctx->connectionName(), false));
        QVERIFY(q.exec(QStringLiteral("UPDATE users SET locked_until = '2000-01-01T00:00:00Z'")));
        QVERIFY(auth.login(QStringLiteral("cust@test.pk"), QStringLiteral("CustPass1")).isOk());
    }

    void onlyOneFirstAdmin()
    {
        auto second = m_b->ctx->auth().createFirstAdmin(QStringLiteral("Evil"), QStringLiteral("evil@test.pk"),
                                                       QStringLiteral("EvilPass1"));
        QVERIFY(!second.isOk());
        QVERIFY(m_b->ctx->auth().hasAdmin().value());
    }

    void changePassword()
    {
        auto &auth = m_b->ctx->auth();
        QVERIFY(!auth.changePassword(m_b->customer, QStringLiteral("wrong"), QStringLiteral("NewPass99")).isOk());
        QVERIFY(!auth.changePassword(m_b->customer, QStringLiteral("CustPass1"), QStringLiteral("weak")).isOk());
        QVERIFY(auth.changePassword(m_b->customer, QStringLiteral("CustPass1"), QStringLiteral("NewPass99")).isOk());
        QVERIFY(!auth.login(QStringLiteral("cust@test.pk"), QStringLiteral("CustPass1")).isOk());
        QVERIFY(auth.login(QStringLiteral("cust@test.pk"), QStringLiteral("NewPass99")).isOk());
    }

    void securityEventsAreAudited()
    {
        m_b->ctx->auth().login(QStringLiteral("cust@test.pk"), QStringLiteral("CustPass1"));
        QVERIFY(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM audit_logs WHERE action = 'login'")).toInt() >= 1);
        QVERIFY(m_b->scalar(QStringLiteral("SELECT COUNT(*) FROM audit_logs WHERE action = 'admin_created'")).toInt() == 1);
    }

    void sampleCustomersAreAdminOnlyAndSkipPasswordRules()
    {
        auto &auth = m_b->ctx->auth();
        // Normal registration still enforces the 8-character rule...
        QVERIFY(!auth.registerCustomer(QStringLiteral("Ali Haider"), QStringLiteral("ali@test.pk"),
                                       QStringLiteral("Ali123")).isOk());
        // ...customers cannot create sample accounts...
        QVERIFY(!auth.createSampleCustomer(m_b->customer, QStringLiteral("Ali Haider"), QStringLiteral("ali@test.pk"),
                                           QStringLiteral("Ali123")).isOk());
        // ...but the admin's sample data can use the simple "<FirstName>123" passwords.
        auto created = auth.createSampleCustomer(m_b->admin, QStringLiteral("Ali Haider"), QStringLiteral("ali@test.pk"),
                                                 QStringLiteral("Ali123"));
        QVERIFY2(created.isOk(), qPrintable(created.error()));
        QVERIFY(created.value().isCustomer());
        QVERIFY(auth.login(QStringLiteral("ali@test.pk"), QStringLiteral("Ali123")).isOk());
        QVERIFY(!auth.login(QStringLiteral("ali@test.pk"), QStringLiteral("ali123")).isOk());
        // Duplicate emails and bad emails are still refused.
        QVERIFY(!auth.createSampleCustomer(m_b->admin, QStringLiteral("Ali Two"), QStringLiteral("ALI@test.pk"),
                                           QStringLiteral("Ali123")).isOk());
        QVERIFY(!auth.createSampleCustomer(m_b->admin, QStringLiteral("No Email"), QStringLiteral("not-an-email"),
                                           QStringLiteral("No123")).isOk());
    }

    void sampleCustomerHashIsUpgradedOnFirstLogin()
    {
        // A back end with production-like strength above the sample strength.
        AppConfig cfg;
        cfg.databasePath = QStringLiteral(":memory:");
        cfg.imageDirectory = m_b->imageDir.path();
        cfg.passwordIterations = AuthService::kSampleIterations * 2;
        m_b->ctx = AppContext::create(cfg).takeValue();
        auto &auth = m_b->ctx->auth();
        auto admin = auth.createFirstAdmin(QStringLiteral("Owner"), QStringLiteral("owner@test.pk"),
                                           QStringLiteral("OwnerPass1"));
        QVERIFY(admin.isOk());
        QVERIFY(auth.createSampleCustomer(admin.value(), QStringLiteral("Sana Butt"), QStringLiteral("sana@test.pk"),
                                          QStringLiteral("Sana123")).isOk());

        const QString query = QStringLiteral("SELECT password_hash FROM users WHERE email = 'sana@test.pk'");
        QVERIFY(m_b->scalar(query).toString().contains(QStringLiteral("$%1$").arg(AuthService::kSampleIterations)));
        QVERIFY(auth.login(QStringLiteral("sana@test.pk"), QStringLiteral("Sana123")).isOk());
        QVERIFY(m_b->scalar(query).toString().contains(QStringLiteral("$%1$").arg(cfg.passwordIterations)));
        QVERIFY(auth.login(QStringLiteral("sana@test.pk"), QStringLiteral("Sana123")).isOk());
    }

private:
    std::unique_ptr<test::Backend> m_b;
};

QTEST_GUILESS_MAIN(TestAuth)
#include "tst_auth.moc"
