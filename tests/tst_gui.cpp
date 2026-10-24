// GUI journeys: sign in, browse, add to cart, checkout, and the admin pages.
// Runs offscreen (no window appears). Set SHOPHUB_SCREENSHOT_DIR to also save
// PNG screenshots of every screen it visits.

#include "TestSupport.h"

#include "services/DemoSeeder.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/dialogs/AdminDialogs.h"
#include "ui/dialogs/AuthDialog.h"
#include "ui/dialogs/KhataDialogs.h"
#include "ui/dialogs/ShopDialogs.h"
#include "ui/pages/Page.h"
#include "ui/widgets/ProductCard.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
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

QRadioButton *radioWithText(QWidget *root, const QString &text)
{
    for (QRadioButton *r : root->findChildren<QRadioButton *>())
        if (r->text() == text)
            return r;
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

    void customerBuysOnKhata()
    {
        MainWindow window(*m_ctx, m_customer);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.page(QStringLiteral("khata")));
        const KhataAccount before = m_ctx->khata().myAccount(m_customer).value();
        QVERIFY(before.creditLimit > Money()); // the hinted sample account has a khata

        // Too big for the available credit: the khata option is switched off.
        ProductFilter f;
        f.text = QStringLiteral("Atomic Habits");
        const qint64 book = m_ctx->catalog().search(m_customer, f).value().items.first().id;
        f.text = QStringLiteral("Air Fryer");
        auto big = m_ctx->catalog().search(m_customer, f).value().items;
        QVERIFY(!big.isEmpty());
        QVERIFY(big.first().price > before.available());
        QVERIFY(m_ctx->cart().add(m_customer, big.first().id, 1).isOk());
        {
            CheckoutDialog checkout(window.uiContext(), &window);
            checkout.show();
            QVERIFY(QTest::qWaitForWindowExposed(&checkout));
            QVERIFY(!radioWithText(&checkout, QStringLiteral("Khata (pay later)"))->isEnabled());
        }
        QVERIFY(m_ctx->cart().clear(m_customer).isOk());

        // Small enough: pay with khata.
        QVERIFY(m_ctx->cart().add(m_customer, book, 1).isOk());
        CheckoutDialog checkout(window.uiContext(), &window);
        checkout.resize(960, 640);
        checkout.show();
        QVERIFY(QTest::qWaitForWindowExposed(&checkout));
        QRadioButton *khata = radioWithText(&checkout, QStringLiteral("Khata (pay later)"));
        QVERIFY(khata && khata->isEnabled());
        QTest::mouseClick(khata, Qt::LeftButton);
        QVERIFY(khata->isChecked());
        shot(&checkout, "07b-checkout-khata");
        QPushButton *place = nullptr;
        for (QPushButton *b : checkout.findChildren<QPushButton *>())
            if (b->text().startsWith(QStringLiteral("Place order")))
                place = b;
        QTest::mouseClick(place, Qt::LeftButton);
        QCOMPARE(checkout.result(), int(QDialog::Accepted));
        QCOMPARE(checkout.placedOrder().paymentMethod, payment::kKhata);
        QCOMPARE(m_ctx->khata().myAccount(m_customer).value().balance, before.balance + checkout.placedOrder().total);

        window.navigate(QStringLiteral("khata"));
        QTest::qWait(20);
        auto *ledger = window.page(QStringLiteral("khata"))->findChild<QTableView *>();
        QVERIFY(ledger);
        QVERIFY(ledger->model()->rowCount() >= 4);
        QCOMPARE(ledger->model()->index(0, 2).data().toString(), checkout.placedOrder().orderNumber);
        shot(&window, "08b-my-khata");
    }

    void adminRecordsKhataPayment()
    {
        MainWindow window(*m_ctx, m_admin);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.navigate(QStringLiteral("khata"));
        Page *page = window.page(QStringLiteral("khata"));
        auto tables = page->findChildren<QTableView *>();
        QCOMPARE(tables.size(), 2);
        QCOMPARE(tables.first()->model()->rowCount(), 50); // every customer is listed
        QVERIFY(tables.last()->model()->rowCount() > 0);    // ledger of the top debtor
        QTest::qWait(20);
        shot(&window, "17-admin-khata");

        page->applySearch(QStringLiteral("ayesha.khan"));
        QCOMPARE(tables.first()->model()->rowCount(), 1);

        const KhataAccount ayesha = m_ctx->khata().account(m_admin, m_customer.userId()).value();
        KhataDialog dialog(window.uiContext(), ayesha, KhataDialog::Mode::RecordPayment, &window);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QLineEdit *amount = fieldWithPlaceholder(&dialog, QStringLiteral("Amount received"));
        QVERIFY(amount);
        amount->setText(QStringLiteral("5000"));
        shot(&dialog, "18-khata-payment");
        QTest::mouseClick(buttonWithText(&dialog, QStringLiteral("Record payment")), Qt::LeftButton);
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(m_ctx->khata().account(m_admin, m_customer.userId()).value().balance,
                 ayesha.balance - Money::fromRupees(5000));

        // Asking for more than is owed is refused with a message, and nothing is saved.
        const KhataAccount after = m_ctx->khata().account(m_admin, m_customer.userId()).value();
        KhataDialog tooMuch(window.uiContext(), after, KhataDialog::Mode::RecordPayment, &window);
        tooMuch.show();
        fieldWithPlaceholder(&tooMuch, QStringLiteral("Amount received"))->setText(QStringLiteral("999999"));
        QTest::mouseClick(buttonWithText(&tooMuch, QStringLiteral("Record payment")), Qt::LeftButton);
        QVERIFY(tooMuch.result() != QDialog::Accepted);
        QCOMPARE(m_ctx->khata().account(m_admin, m_customer.userId()).value().balance, after.balance);
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

    void wholesalePricesFromAdminToCheckout()
    {
        ProductFilter f;
        f.text = QStringLiteral("Match Football");
        const Product football = m_ctx->catalog().search(m_admin, f).value().items.first();
        QCOMPARE(football.wholesaleFrom, 6); // sample data: 6+ 5%, 12+ 10%, 24+ 15%

        // ---- admin: the quantity prices editor
        {
            MainWindow window(*m_ctx, m_admin);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.navigate(QStringLiteral("products"));
            QVERIFY(buttonWithText(&window, QStringLiteral("Quantity prices...")));

            PriceTiersDialog dialog(window.uiContext(), football, &window);
            QVERIFY(dialog.isLoaded());
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            auto spins = dialog.findChildren<QSpinBox *>();
            QCOMPARE(spins.size(), PriceTiersDialog::kRows);
            QCOMPARE(spins.at(0)->value(), 6); // the saved tiers are filled in, in order
            QCOMPARE(spins.at(2)->value(), 24);
            QCOMPARE(spins.at(3)->value(), 0);

            // A bigger quantity with a smaller discount is refused with a message.
            dialog.setRow(3, 50, QStringLiteral("12"));
            dialog.save();
            QVERIFY(dialog.result() != QDialog::Accepted);
            bool errorShown = false;
            for (QLabel *l : dialog.findChildren<QLabel *>())
                errorShown = errorShown || (l->isVisible() && l->text().contains(QStringLiteral("must not cost more")));
            QVERIFY(errorShown);

            // 60 or more at 50% off is below the cost price: allowed, with a warning.
            dialog.setRow(3, 50, QStringLiteral("20"));
            dialog.setRow(4, 60, QStringLiteral("50"));
            bool warningShown = false;
            for (QLabel *l : dialog.findChildren<QLabel *>())
                warningShown = warningShown || (l->isVisible() && l->text().startsWith(QStringLiteral("Below the cost price")));
            QVERIFY(warningShown);
            shot(&dialog, "19-quantity-prices");
            dialog.save();
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QCOMPARE(m_ctx->catalog().priceTiers(m_admin, football.id).value().size(), 5);
        }

        // ---- customer: badge, product page, cart hint, checkout
        MainWindow window(*m_ctx, m_customer);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.navigate(QStringLiteral("shop"));
        QTest::qWait(20);
        // 6 sample products have quantity prices; the first page of the shop shows most of them.
        bool badgeShown = false;
        for (ProductCard *card : window.findChildren<ProductCard *>())
            for (QLabel *l : card->findChildren<QLabel *>())
                badgeShown = badgeShown || l->text().startsWith(QStringLiteral("Wholesale "));
        QVERIFY(badgeShown);

        {
            ProductDetailsDialog details(window.uiContext(), football.id, &window);
            QVERIFY(details.isLoaded());
            details.show();
            QVERIFY(QTest::qWaitForWindowExposed(&details));
            bool tableShown = false;
            for (QLabel *l : details.findChildren<QLabel *>())
                tableShown = tableShown || l->text() == QStringLiteral("Buy more, pay less");
            QVERIFY(tableShown);
            shot(&details, "20-product-wholesale");
        }

        QVERIFY(m_ctx->cart().clear(m_customer).isOk());
        QVERIFY(m_ctx->cart().add(m_customer, football.id, 11).isOk()); // 6+ applies, 12+ is one away
        window.navigate(QStringLiteral("cart"));
        QTest::qWait(20);
        QPushButton *addOne = buttonWithText(&window, QStringLiteral("Add 1"));
        QVERIFY(addOne);
        QTest::mouseClick(addOne, Qt::LeftButton);
        QTest::qWait(20);
        const CartItem line = m_ctx->cart().items(m_customer).value().first();
        QCOMPARE(line.quantity, 12);
        QCOMPARE(line.unitPrice, Money::fromPaisa(296910)); // Rs 3,299 less 10%
        bool savingsShown = false;
        for (QLabel *l : window.findChildren<QLabel *>())
            savingsShown = savingsShown || (l->isVisible() && l->text().startsWith(QStringLiteral("You save")));
        QVERIFY(savingsShown);
        shot(&window, "21-cart-wholesale");

        CheckoutDialog checkout(window.uiContext(), &window);
        checkout.resize(960, 620);
        checkout.show();
        QVERIFY(QTest::qWaitForWindowExposed(&checkout));
        QPushButton *place = nullptr;
        for (QPushButton *b : checkout.findChildren<QPushButton *>())
            if (b->text().startsWith(QStringLiteral("Place order")))
                place = b;
        QVERIFY(place);
        QTest::mouseClick(place, Qt::LeftButton);
        QCOMPARE(checkout.result(), int(QDialog::Accepted));
        QCOMPARE(checkout.placedOrder().subtotal, Money::fromPaisa(296910 * 12));
        QCOMPARE(checkout.placedOrder().items.first().unitPrice, Money::fromPaisa(296910));
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

// Uses Qt's invisible "offscreen" display when this Qt can really load it, and the
// normal display otherwise (the windows then appear briefly while the test runs).
// Checked at run time, because a copy of Qt placed next to the exe (as windeployqt
// does on Windows) can only load the plugins that were copied with it.
int main(int argc, char **argv)
{
    {
        QCoreApplication probe(argc, argv);
        bool haveOffscreen = false;
        for (const QString &path : QCoreApplication::libraryPaths())
            haveOffscreen = haveOffscreen || !QDir(path + QStringLiteral("/platforms"))
                                                  .entryList({QStringLiteral("*qoffscreen*")}).isEmpty();
        if (haveOffscreen && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
        else if (!haveOffscreen && qgetenv("QT_QPA_PLATFORM") == "offscreen")
            qunsetenv("QT_QPA_PLATFORM");
    }
    QApplication app(argc, argv);
    TestGui test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_gui.moc"
