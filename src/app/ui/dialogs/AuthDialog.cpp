#include "ui/dialogs/AuthDialog.h"

#include "services/AppContext.h"
#include "services/DemoSeeder.h"
#include "ui/UiCommon.h"

#include <QApplication>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace shophub::ui {

namespace {

QLineEdit *field(const QString &placeholder, bool password = false)
{
    auto *e = new QLineEdit();
    e->setPlaceholderText(placeholder);
    e->setMinimumHeight(38);
    if (password)
        e->setEchoMode(QLineEdit::Password);
    return e;
}

QLabel *errorLabel()
{
    auto *l = makeLabel(QString(), "error");
    l->setWordWrap(true);
    l->hide();
    return l;
}

void setError(QLabel *label, const QString &message)
{
    label->setText(message);
    label->setVisible(!message.isEmpty());
}

// Wraps a form so it sits centred with a sensible maximum width.
QWidget *centred(QLayout *content)
{
    auto *page = new QWidget();
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(48, 32, 48, 32);
    outer->addStretch();
    auto *box = new QWidget();
    box->setMaximumWidth(380);
    box->setLayout(content);
    outer->addWidget(box, 0, Qt::AlignHCenter);
    outer->addStretch();
    return page;
}

} // namespace

AuthDialog::AuthDialog(AppContext &app, Mode mode, QWidget *parent)
    : QDialog(parent), m_app(app)
{
    setWindowTitle(mode == Mode::FirstRun ? QStringLiteral("Set up ShopHub") : QStringLiteral("Sign in - ShopHub"));
    setMinimumSize(900, 600);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildBrandPanel());

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildSignInPage());
    m_stack->addWidget(buildRegisterPage());
    m_stack->addWidget(buildFirstRunPage());
    layout->addWidget(m_stack, 1);

    showPage(mode == Mode::FirstRun ? 2 : 0);
}

QWidget *AuthDialog::buildBrandPanel()
{
    auto *panel = new QFrame();
    panel->setObjectName(QStringLiteral("BrandPanel"));
    panel->setFixedWidth(360);
    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(40, 44, 40, 32);
    v->setSpacing(10);

    auto *logo = new QLabel(QStringLiteral("ShopHub"));
    logo->setStyleSheet(QStringLiteral("font-size: 30px; font-weight: 800;"));
    v->addWidget(logo);
    auto *tagline = new QLabel(QStringLiteral("Everything you need,\ndelivered to your door."));
    tagline->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600;"));
    v->addWidget(tagline);
    v->addSpacing(24);

    for (const QString &point : {QStringLiteral("Cash on Delivery across Pakistan"),
                                 QStringLiteral("Track every order step by step"),
                                 QStringLiteral("Your cart is saved between visits")}) {
        auto *l = new QLabel(QStringLiteral("✓  ") + point);
        l->setStyleSheet(QStringLiteral("font-size: 13px;"));
        v->addWidget(l);
    }
    v->addStretch();
    if (!QApplication::applicationVersion().isEmpty()) {
        auto *version = new QLabel(QStringLiteral("Version %1").arg(QApplication::applicationVersion()));
        version->setStyleSheet(QStringLiteral("font-size: 11px;"));
        v->addWidget(version);
    }
    return panel;
}

QWidget *AuthDialog::buildSignInPage()
{
    auto *v = new QVBoxLayout();
    v->setSpacing(10);
    v->addWidget(makeLabel(QStringLiteral("Welcome back"), "h1"));
    v->addWidget(makeLabel(QStringLiteral("Sign in to your ShopHub account."), "muted"));
    v->addSpacing(12);

    m_loginEmail = field(QStringLiteral("Email address"));
    m_loginEmail->setAccessibleName(QStringLiteral("Email address"));
    m_loginPassword = field(QStringLiteral("Password"), true);
    m_loginPassword->setAccessibleName(QStringLiteral("Password"));
    v->addWidget(makeLabel(QStringLiteral("Email"), "h3"));
    v->addWidget(m_loginEmail);
    v->addWidget(makeLabel(QStringLiteral("Password"), "h3"));
    v->addWidget(m_loginPassword);

    auto *show = new QCheckBox(QStringLiteral("Show password"));
    connect(show, &QCheckBox::toggled, this, [this](bool on) {
        m_loginPassword->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    v->addWidget(show);

    m_loginError = errorLabel();
    v->addWidget(m_loginError);

    m_loginButton = makeButton(QStringLiteral("Sign in"), "primary");
    m_loginButton->setMinimumHeight(40);
    m_loginButton->setDefault(true);
    connect(m_loginButton, &QPushButton::clicked, this, &AuthDialog::doSignIn);
    connect(m_loginPassword, &QLineEdit::returnPressed, this, &AuthDialog::doSignIn);
    connect(m_loginEmail, &QLineEdit::returnPressed, m_loginPassword, qOverload<>(&QWidget::setFocus));
    v->addSpacing(6);
    v->addWidget(m_loginButton);

    auto *row = new QHBoxLayout();
    row->addWidget(makeLabel(QStringLiteral("New to ShopHub?"), "muted"));
    auto *toRegister = makeButton(QStringLiteral("Create an account"), "link");
    connect(toRegister, &QPushButton::clicked, this, [this] { showPage(1); });
    row->addWidget(toRegister);
    row->addStretch();
    v->addLayout(row);

    if (DemoSeeder::isLoaded(m_app)) {
        auto *hint = makeLabel(QStringLiteral("Sample customer:  %1\nPassword:  %2\n"
                                              "All %3 sample customers: first name + 123")
                                   .arg(DemoSeeder::kCustomerEmail, DemoSeeder::kCustomerPassword)
                                   .arg(DemoSeeder::sampleCustomers().size()),
                               "hint");
        hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addSpacing(10);
        v->addWidget(hint);
    }
    return centred(v);
}

QWidget *AuthDialog::buildRegisterPage()
{
    auto *v = new QVBoxLayout();
    v->setSpacing(8);
    v->addWidget(makeLabel(QStringLiteral("Create your account"), "h1"));
    v->addWidget(makeLabel(QStringLiteral("It takes less than a minute."), "muted"));
    v->addSpacing(10);

    m_regName = field(QStringLiteral("Full name"));
    m_regEmail = field(QStringLiteral("Email address"));
    m_regPassword = field(QStringLiteral("Password"), true);
    m_regConfirm = field(QStringLiteral("Confirm password"), true);
    for (QLineEdit *e : {m_regName, m_regEmail, m_regPassword, m_regConfirm}) {
        e->setAccessibleName(e->placeholderText());
        v->addWidget(e);
    }
    v->addWidget(makeLabel(QStringLiteral("At least 8 characters, with a letter and a number."), "small"));

    m_regError = errorLabel();
    v->addWidget(m_regError);

    m_regButton = makeButton(QStringLiteral("Create account"), "primary");
    m_regButton->setMinimumHeight(40);
    connect(m_regButton, &QPushButton::clicked, this, &AuthDialog::doRegister);
    connect(m_regConfirm, &QLineEdit::returnPressed, this, &AuthDialog::doRegister);
    v->addSpacing(6);
    v->addWidget(m_regButton);

    auto *row = new QHBoxLayout();
    row->addWidget(makeLabel(QStringLiteral("Already have an account?"), "muted"));
    auto *toLogin = makeButton(QStringLiteral("Sign in"), "link");
    connect(toLogin, &QPushButton::clicked, this, [this] { showPage(0); });
    row->addWidget(toLogin);
    row->addStretch();
    v->addLayout(row);
    return centred(v);
}

QWidget *AuthDialog::buildFirstRunPage()
{
    auto *v = new QVBoxLayout();
    v->setSpacing(8);
    v->addWidget(makeLabel(QStringLiteral("Set up your store"), "h1"));
    auto *intro = makeLabel(QStringLiteral("Create the administrator account. You will use it to manage "
                                           "products, stock and orders."),
                            "muted");
    intro->setWordWrap(true);
    v->addWidget(intro);
    v->addSpacing(10);

    m_adminName = field(QStringLiteral("Your full name"));
    m_adminEmail = field(QStringLiteral("Admin email address"));
    m_adminPassword = field(QStringLiteral("Password"), true);
    m_adminConfirm = field(QStringLiteral("Confirm password"), true);
    for (QLineEdit *e : {m_adminName, m_adminEmail, m_adminPassword, m_adminConfirm}) {
        e->setAccessibleName(e->placeholderText());
        v->addWidget(e);
    }
    v->addWidget(makeLabel(QStringLiteral("At least 8 characters, with a letter and a number."), "small"));

    m_loadDemo = new QCheckBox(QStringLiteral("Add sample products, orders and 50 customer accounts"));
    m_loadDemo->setChecked(true);
    v->addSpacing(4);
    v->addWidget(m_loadDemo);
    auto *demoNote = makeLabel(QStringLiteral("Recommended for trying ShopHub. Sample passwords are simple, so skip this for a real shop."), "small");
    demoNote->setWordWrap(true);
    v->addWidget(demoNote);

    m_adminError = errorLabel();
    v->addWidget(m_adminError);

    m_adminButton = makeButton(QStringLiteral("Create admin account"), "primary");
    m_adminButton->setMinimumHeight(40);
    connect(m_adminButton, &QPushButton::clicked, this, &AuthDialog::doFirstRun);
    v->addSpacing(6);
    v->addWidget(m_adminButton);
    return centred(v);
}

void AuthDialog::showPage(int index)
{
    m_stack->setCurrentIndex(index);
    QLineEdit *focus = index == 0 ? m_loginEmail : index == 1 ? m_regName : m_adminName;
    focus->setFocus();
}

void AuthDialog::setBusy(QPushButton *button, bool busy, const QString &idleText)
{
    button->setEnabled(!busy);
    button->setText(busy ? QStringLiteral("Please wait...") : idleText);
    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    } else {
        QApplication::restoreOverrideCursor();
    }
}

void AuthDialog::doSignIn()
{
    if (!m_loginButton->isEnabled())
        return;
    setBusy(m_loginButton, true, QStringLiteral("Sign in"));
    auto result = m_app.auth().login(m_loginEmail->text(), m_loginPassword->text());
    setBusy(m_loginButton, false, QStringLiteral("Sign in"));
    if (!result) {
        setError(m_loginError, result.error());
        m_loginPassword->selectAll();
        m_loginPassword->setFocus();
        return;
    }
    m_session = result.value();

    // Sample data from an older ShopHub gets what newer versions added (once, admin only).
    if (m_session.isAdmin() && DemoSeeder::needsUpgrade(m_app)) {
        setBusy(m_loginButton, true, QStringLiteral("Sign in"));
        const Status upgraded = DemoSeeder::upgrade(m_app, m_session);
        setBusy(m_loginButton, false, QStringLiteral("Sign in"));
        if (!upgraded)
            showInfo(this, QStringLiteral("Sample data"),
                     QStringLiteral("You are signed in, but the sample data could not be updated:\n%1").arg(upgraded.error()));
    }
    accept();
}

void AuthDialog::doRegister()
{
    if (!m_regButton->isEnabled())
        return;
    if (m_regPassword->text() != m_regConfirm->text()) {
        setError(m_regError, QStringLiteral("The passwords do not match."));
        return;
    }
    setBusy(m_regButton, true, QStringLiteral("Create account"));
    auto result = m_app.auth().registerCustomer(m_regName->text(), m_regEmail->text(), m_regPassword->text());
    setBusy(m_regButton, false, QStringLiteral("Create account"));
    if (!result) {
        setError(m_regError, result.error());
        return;
    }
    m_session = result.value();
    accept();
}

void AuthDialog::doFirstRun()
{
    if (!m_adminButton->isEnabled())
        return;
    if (m_adminPassword->text() != m_adminConfirm->text()) {
        setError(m_adminError, QStringLiteral("The passwords do not match."));
        return;
    }
    setBusy(m_adminButton, true, QStringLiteral("Create admin account"));
    auto result = m_app.auth().createFirstAdmin(m_adminName->text(), m_adminEmail->text(), m_adminPassword->text());
    if (result && m_loadDemo->isChecked()) {
        const Status seeded = DemoSeeder::seed(m_app, result.value());
        if (!seeded)
            showInfo(this, QStringLiteral("Sample data"),
                     QStringLiteral("Your admin account was created, but the sample data could not be loaded:\n%1")
                         .arg(seeded.error()));
    }
    setBusy(m_adminButton, false, QStringLiteral("Create admin account"));
    if (!result) {
        setError(m_adminError, result.error());
        return;
    }
    m_session = result.value();
    accept();
}

} // namespace shophub::ui
