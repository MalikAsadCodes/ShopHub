// GUI journeys: sign in, browse, add to cart, checkout, and the admin pages.
// Runs offscreen (no window appears). Set SHOPHUB_SCREENSHOT_DIR to also save
// PNG screenshots of every screen it visits.

#include "TestSupport.h"

#include "services/DemoSeeder.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/dialogs/AdminDialogs.h"
#include "ui/dialogs/AuthDialog.h"
#include "ui/dialogs/ShopDialogs.h"
#include "ui/pages/Page.h"
#include "ui/widgets/ProductCard.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>

using namespace shophub;
using namespace shophub::ui;

namespace {

QPushButton *buttonWithText(QWidget *root, const QString &text)
{
    for (QPushButton *b : root->findChildren<QPushButton *>())
        if (b->text() == text && b->isVisible())
            return b;
    return nullptr;
}

QLineEdit *fieldWithPlaceholder(QWidget *root, const QString &placeholder)
{
    for (QLineEdit *e : root->findChildren<QLineEdit *>())
        if (e->placeholderText() == placeholder && e->isVisible())
            return e;
    return nullptr;
}

} // namespace

class TestGui : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Theme::apply(ThemeMode::Light);
        AppConfig cfg;
        cfg.databasePath = QStringLiteral(":memory:");
        cfg.imageDirectory = m_images.path();
        cfg.passwordIterations = 1000;
        auto created = AppContext::create(cfg);
        QVERIFY2(created.isOk(), qPrintable(created.error()));
        m_ctx = created.takeValue();

        // First-run dialog creates the admin and loads the demo data.
        AuthDialog setup(*m_ctx, AuthDialog::Mode::FirstRun);
        setup.resize(940, 620);
        setup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&setup));
        shot(&setup, "01-first-run");
        QTest::keyClicks(fieldWithPlaceholder(&setup, QStringLiteral("Your full name")), QStringLiteral("Hammad Ahmed"));
        QTest::keyClicks(fieldWithPlaceholder(&setup, QStringLiteral("Admin email address")), QStringLiteral("admin@shophub.pk"));
        QTest::keyClicks(fieldWithPlaceholder(&setup, QStringLiteral("Password")), QStringLiteral("AdminPass1"));
        QTest::keyClicks(fieldWithPlaceholder(&setup, QStringLiteral("Confirm password")), QStringLiteral("AdminPass1"));
        QTest::mouseClick(buttonWithText(&setup, QStringLiteral("Create admin account")), Qt::LeftButton);
        QCOMPARE(setup.result(), int(QDialog::Accepted));
        QVERIFY(setup.session().isAdmin());
        m_admin = setup.session();
        QVERIFY(DemoSeeder::isLoaded(*m_ctx));
    }

    void wrongPasswordShowsError()
    {
        AuthDialog dialog(*m_ctx, AuthDialog::Mode::SignIn);
        dialog.show();
        QTest::keyClicks(fieldWithPlaceholder(&dialog, QStringLiteral("Email address")), DemoSeeder::kCustomerEmail);
        QTest::keyClicks(fieldWithPlaceholder(&dialog, QStringLiteral("Password")), QStringLiteral("wrong-pass9"));
        QTest::mouseClick(buttonWithText(&dialog, QStringLiteral("Sign in")), Qt::LeftButton);
        QVERIFY(dialog.result() != QDialog::Accepted);
        bool errorShown = false;
        for (QLabel *l : dialog.findChildren<QLabel *>())
            errorShown = errorShown || (l->isVisible() && l->text() == QStringLiteral("Invalid email or password."));
        QVERIFY(errorShown);
    }

    void customerSignsIn()
    {
        AuthDialog dialog(*m_ctx, AuthDialog::Mode::SignIn);
        dialog.resize(940, 620);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        shot(&dialog, "02-sign-in");
        QTest::keyClicks(fieldWithPlaceholder(&dialog, QStringLiteral("Email address")), DemoSeeder::kCustomerEmail);
        QTest::keyClicks(fieldWithPlaceholder(&dialog, QStringLiteral("Password")), DemoSeeder::kCustomerPassword);
        QTest::keyClick(fieldWithPlaceholder(&dialog, QStringLiteral("Password")), Qt::Key_Return);
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(dialog.session().isCustomer());
        m_customer = dialog.session();
    }

    void customerShopsAndChecksOut()
    {
        MainWindow window(*m_ctx, m_customer);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.currentPageKey(), QStringLiteral("home"));
        QVERIFY(!window.page(QStringLiteral("dashboard"))); // no admin pages for customers
        shot(&window, "03-customer-home");

        window.navigate(QStringLiteral("shop"));
        QTest::qWait(20);
        shot(&window, "04-shop");
        auto cards = window.findChildren<ProductCard *>();
        QVERIFY(cards.size() >= 20);

        const int before = m_ctx->cart().itemCount(m_customer).value();
        QPushButton *add = nullptr;
        for (ProductCard *card : cards) {
            for (QPushButton *b : card->findChildren<QPushButton *>())
                if (b->text() == QStringLiteral("Add to cart") && b->isEnabled())
                    add = b;
            if (add)
                break;
        }
        QVERIFY(add);
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(m_ctx->cart().itemCount(m_customer).value(), before + 1);

        // Product details dialog
        ProductDetailsDialog details(window.uiContext(), cards.first()->productId(), &window);
        QVERIFY(details.isLoaded());
        details.show();
        QVERIFY(QTest::qWaitForWindowExposed(&details));
        shot(&details, "05-product-details");
        details.close();

        // Add a second item through the service so the cart has two lines.
        ProductFilter f;
        f.text = QStringLiteral("Atomic Habits");
        const qint64 book = m_ctx->catalog().search(m_customer, f).value().items.first().id;
        QVERIFY(m_ctx->cart().add(m_customer, book, 2).isOk());

        window.navigate(QStringLiteral("cart"));
        QTest::qWait(20);
        shot(&window, "06-cart");
        QVERIFY(buttonWithText(&window, QStringLiteral("Proceed to checkout")));

        const int ordersBefore = m_ctx->orders().myOrders(m_customer).value().size();
        CheckoutDialog checkout(window.uiContext(), &window);
        checkout.resize(960, 620);
        checkout.show();
        QVERIFY(QTest::qWaitForWindowExposed(&checkout));
        shot(&checkout, "07-checkout");
        QPushButton *place = nullptr;
        for (QPushButton *b : checkout.findChildren<QPushButton *>())
            if (b->text().startsWith(QStringLiteral("Place order")))
                place = b;
        QVERIFY(place);
        QTest::mouseClick(place, Qt::LeftButton);
        QCOMPARE(checkout.result(), int(QDialog::Accepted));
        QVERIFY(checkout.placedOrder().orderNumber.startsWith(QStringLiteral("SH-")));
        QCOMPARE(m_ctx->orders().myOrders(m_customer).value().size(), ordersBefore + 1);
        QCOMPARE(m_ctx->cart().itemCount(m_customer).value(), 0);

        window.navigate(QStringLiteral("orders"));
        QTest::qWait(20);
        shot(&window, "08-my-orders");

        OrderDetailsDialog orderDialog(window.uiContext(), checkout.placedOrder().id, &window);
        QVERIFY(orderDialog.isLoaded());
        orderDialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&orderDialog));
        shot(&orderDialog, "09-order-details");
    }

    void adminPagesWork()
    {
        MainWindow window(*m_ctx, m_admin);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.currentPageKey(), QStringLiteral("dashboard"));
        QVERIFY(!window.page(QStringLiteral("cart")));
        QTest::qWait(20);
        shot(&window, "10-admin-dashboard");

        window.navigate(QStringLiteral("admin-orders"));
        auto *ordersTable = window.page(QStringLiteral("admin-orders"))->findChild<QTableView *>();
        QVERIFY(ordersTable);
        QVERIFY(ordersTable->model()->rowCount() >= 9);
        shot(&window, "11-admin-orders");

        window.navigate(QStringLiteral("products"));
        auto *productsTable = window.page(QStringLiteral("products"))->findChild<QTableView *>();
        QCOMPARE(productsTable->model()->rowCount(), 27);
        shot(&window, "12-admin-products");

        window.navigate(QStringLiteral("inventory"));
        QTest::qWait(20);
        shot(&window, "13-inventory");

        ProductEditDialog edit(window.uiContext(), productsTable->model()->index(0, 0).data(Qt::UserRole + 1).toLongLong(), &window);
        QVERIFY(edit.isLoaded());
        edit.show();
        QVERIFY(QTest::qWaitForWindowExposed(&edit));
        shot(&edit, "14-edit-product");
        edit.close();

        Theme::apply(ThemeMode::Dark);
        window.navigate(QStringLiteral("dashboard"));
        QTest::qWait(20);
        shot(&window, "15-dashboard-dark");
        window.navigate(QStringLiteral("shop"));
        QTest::qWait(20);
        shot(&window, "16-shop-dark");
        Theme::apply(ThemeMode::Light);
    }

    void cleanupTestCase()
    {
        m_ctx.reset();
    }

private:
    void shot(QWidget *w, const char *name)
    {
        const QString dir = qEnvironmentVariable("SHOPHUB_SCREENSHOT_DIR");
        if (dir.isEmpty())
            return;
        QDir().mkpath(dir);
        QApplication::processEvents();
        w->grab().save(QDir(dir).filePath(QString::fromLatin1(name) + QStringLiteral(".png")));
    }

    QTemporaryDir m_images;
    std::unique_ptr<AppContext> m_ctx;
    Session m_admin;
    Session m_customer;
};

QTEST_MAIN(TestGui)
#include "tst_gui.moc"
