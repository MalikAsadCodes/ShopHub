#include "ui/MainWindow.h"

#include "services/AppContext.h"
#include "ui/Theme.h"
#include "ui/dialogs/ShopDialogs.h"
#include "ui/pages/AdminPages.h"
#include "ui/pages/CustomerPages.h"
#include "ui/pages/KhataPages.h"
#include "ui/pages/QistPages.h"
#include "ui/widgets/Widgets.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace shophub::ui {

MainWindow::MainWindow(AppContext &app, const Session &session, QWidget *parent)
    : QMainWindow(parent), m_events(std::make_unique<AppEvents>()), m_ui{app, session, m_events.get()}
{
    setWindowTitle(session.isAdmin() ? QStringLiteral("ShopHub - Store admin") : QStringLiteral("ShopHub"));
    setMinimumSize(1120, 720);
    resize(1320, 840);

    auto *central = new QWidget(this);
    auto *row = new QHBoxLayout(central);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(buildSidebar());

    auto *right = new QVBoxLayout();
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    right->addWidget(buildTopBar());
    m_stack = new QStackedWidget();
    right->addWidget(m_stack, 1);
    row->addLayout(right, 1);
    setCentralWidget(central);

    if (session.isAdmin()) {
        addPage(QStringLiteral("dashboard"), QStringLiteral("Dashboard"), new DashboardPage(m_ui));
        addPage(QStringLiteral("admin-orders"), QStringLiteral("Orders"), new AdminOrdersPage(m_ui));
        addPage(QStringLiteral("khata"), QStringLiteral("Khata"), new KhataPage(m_ui));
        addPage(QStringLiteral("qist"), QStringLiteral("Qist"), new QistPage(m_ui));
        addPage(QStringLiteral("products"), QStringLiteral("Products"), new AdminProductsPage(m_ui));
        addPage(QStringLiteral("inventory"), QStringLiteral("Inventory"), new InventoryPage(m_ui));
        auto *section = makeLabel(QStringLiteral("STOREFRONT"));
        section->setProperty("sidebar", QStringLiteral("section"));
        m_navLayout->addSpacing(14);
        m_navLayout->addWidget(section);
        m_shop = new ShopPage(m_ui);
        addPage(QStringLiteral("shop"), QStringLiteral("Preview shop"), m_shop);
    } else {
        addPage(QStringLiteral("home"), QStringLiteral("Home"), new HomePage(m_ui));
        m_shop = new ShopPage(m_ui);
        addPage(QStringLiteral("shop"), QStringLiteral("Shop"), m_shop);
        addPage(QStringLiteral("cart"), QStringLiteral("Cart"), new CartPage(m_ui));
        addPage(QStringLiteral("orders"), QStringLiteral("My orders"), new MyOrdersPage(m_ui));
        addPage(QStringLiteral("khata"), QStringLiteral("My khata"), new MyKhataPage(m_ui));
        addPage(QStringLiteral("qist"), QStringLiteral("My qist"), new MyQistPage(m_ui));
    }

    // Events from pages and dialogs
    connect(m_events.get(), &AppEvents::toast, this, [this](const QString &msg) { Toast::show(this, msg); });
    connect(m_events.get(), &AppEvents::cartChanged, this, [this] {
        updateCartBadge();
        if (currentPageKey() == QLatin1String("cart"))
            refreshCurrent();
    });
    connect(m_events.get(), &AppEvents::ordersChanged, this, [this] {
        updateCartBadge();
        refreshCurrent();
    });
    connect(m_events.get(), &AppEvents::catalogChanged, this, &MainWindow::refreshCurrent);
    connect(m_events.get(), &AppEvents::khataChanged, this, &MainWindow::refreshCurrent);
    connect(m_events.get(), &AppEvents::openProduct, this, &MainWindow::openProduct);
    connect(m_events.get(), &AppEvents::navigate, this, [this](const QString &key) { navigate(key); });
    connect(m_events.get(), &AppEvents::openShop, this, [this](qint64 categoryId, const QString &text) {
        navigate(QStringLiteral("shop"), false);
        m_shop->showCategory(categoryId, text);
    });

    // Keyboard shortcuts: Ctrl+F search, F5 refresh, Alt+1..n pages
    auto *find = new QShortcut(QKeySequence::Find, this);
    connect(find, &QShortcut::activated, this, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });
    auto *reload = new QShortcut(QKeySequence::Refresh, this);
    connect(reload, &QShortcut::activated, this, &MainWindow::refreshCurrent);
    for (int i = 0; i < m_order.size() && i < 9; ++i) {
        auto *sc = new QShortcut(QKeySequence(Qt::ALT | Qt::Key(Qt::Key_1 + i)), this);
        connect(sc, &QShortcut::activated, this, [this, key = m_order[i]] { navigate(key); });
    }

    updateCartBadge();
    navigate(m_order.first());
}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::buildSidebar()
{
    auto *bar = new QFrame();
    bar->setObjectName(QStringLiteral("Sidebar"));
    constexpr int kSidebarWidth = 232;
    constexpr int kSidebarPadding = 16;
    bar->setFixedWidth(kSidebarWidth);
    auto *v = new QVBoxLayout(bar);
    v->setContentsMargins(kSidebarPadding, 22, kSidebarPadding, 16);
    v->setSpacing(4);

    auto *logoRow = new QHBoxLayout();
    auto *logo = new QLabel(QStringLiteral("ShopHub"));
    logo->setObjectName(QStringLiteral("Logo"));
    logoRow->addWidget(logo);
    if (m_ui.session.isAdmin())
        logoRow->addWidget(makeBadge(QStringLiteral("ADMIN"), QStringLiteral("accent")), 0, Qt::AlignVCenter);
    logoRow->addStretch();
    v->addLayout(logoRow);
    v->addSpacing(22);

    auto *section = makeLabel(m_ui.session.isAdmin() ? QStringLiteral("MANAGE") : QStringLiteral("SHOP"));
    section->setProperty("sidebar", QStringLiteral("section"));
    v->addWidget(section);
    v->addSpacing(4);

    m_navLayout = new QVBoxLayout();
    m_navLayout->setSpacing(4);
    v->addLayout(m_navLayout);
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    v->addStretch();

    auto *chip = new QFrame();
    chip->setObjectName(QStringLiteral("UserChip"));
    auto *ch = new QHBoxLayout(chip);
    ch->setContentsMargins(10, 10, 10, 10);
    ch->setSpacing(10);
    auto *avatar = new QLabel(initials(m_ui.session.fullName()));
    avatar->setObjectName(QStringLiteral("Avatar"));
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    ch->addWidget(avatar);
    auto *who = new QVBoxLayout();
    who->setSpacing(0);
    auto *name = new QLabel(m_ui.session.fullName());
    name->setStyleSheet(QStringLiteral("color: #FFFFFF; font-weight: 700;"));
    auto *role = new QLabel(m_ui.session.isAdmin() ? QStringLiteral("Store administrator") : m_ui.session.email());
    role->setProperty("sidebar", QStringLiteral("section"));
    who->addWidget(name);
    who->addWidget(role);
    ch->addLayout(who, 1);
    v->addWidget(chip);

    // Long names or emails end in "..." instead of being cut off; the tooltip shows them in full.
    const int textWidth = kSidebarWidth - 2 * kSidebarPadding - 2 * 10 - avatar->width() - ch->spacing() - 4;
    for (QLabel *label : {name, role}) {
        label->ensurePolished();
        const QString full = label->text();
        const QString shown = label->fontMetrics().elidedText(full, Qt::ElideRight, textWidth);
        if (shown != full) {
            label->setText(shown);
            label->setToolTip(full);
        }
    }
    return bar;
}

QWidget *MainWindow::buildTopBar()
{
    auto *bar = new QFrame();
    bar->setObjectName(QStringLiteral("TopBar"));
    bar->setFixedHeight(64);
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(28, 12, 28, 12);
    h->setSpacing(10);

    m_search = new QLineEdit();
    m_search->setObjectName(QStringLiteral("SearchBox"));
    m_search->setPlaceholderText(m_ui.session.isAdmin() ? QStringLiteral("Search products, SKUs or brands   (Ctrl+F)")
                                                        : QStringLiteral("Search for products   (Ctrl+F)"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(380);
    m_search->setMaximumWidth(520);
    m_search->setAccessibleName(QStringLiteral("Search"));
    connect(m_search, &QLineEdit::returnPressed, this, &MainWindow::runSearch);
    h->addWidget(m_search, 1);
    h->addStretch();

    m_themeButton = makeButton(Theme::current() == ThemeMode::Dark ? QStringLiteral("Light mode") : QStringLiteral("Dark mode"),
                               "topbar");
    m_themeButton->setToolTip(QStringLiteral("Switch between light and dark theme"));
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    h->addWidget(m_themeButton);

    if (m_ui.session.isCustomer()) {
        m_cartButton = makeButton(QStringLiteral("Cart"), "topbar");
        m_cartButton->setAccessibleName(QStringLiteral("Open cart"));
        connect(m_cartButton, &QPushButton::clicked, this, [this] { navigate(QStringLiteral("cart")); });
        h->addWidget(m_cartButton);
    }

    auto *account = makeButton(m_ui.session.fullName().split(QLatin1Char(' ')).value(0) + QStringLiteral("  ▾"), "topbar");
    auto *menu = new QMenu(account);
    auto *header = menu->addAction(m_ui.session.email());
    header->setEnabled(false);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Change password..."), this, [this] {
        ChangePasswordDialog dialog(m_ui, this);
        dialog.exec();
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Sign out"), this, &MainWindow::signOut);
    account->setMenu(menu);
    h->addWidget(account);
    return bar;
}

void MainWindow::addPage(const QString &key, const QString &label, Page *page)
{
    page->setParent(m_stack);
    m_stack->addWidget(page);
    m_pages.insert(key, page);
    m_order.append(key);

    auto *button = new QPushButton(label);
    button->setProperty("nav", true);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(QStringLiteral("%1  (Alt+%2)").arg(label).arg(m_order.size()));
    m_navGroup->addButton(button);
    m_navLayout->addWidget(button);
    m_navButtons.insert(key, button);
    connect(button, &QPushButton::clicked, this, [this, key] { navigate(key); });
}

void MainWindow::navigate(const QString &key, bool refresh)
{
    Page *page = m_pages.value(key);
    if (!page)
        return;
    m_stack->setCurrentWidget(page);
    if (QPushButton *b = m_navButtons.value(key))
        b->setChecked(true);
    if (refresh)
        page->refresh();
}

QString MainWindow::currentPageKey() const
{
    return m_pages.key(qobject_cast<Page *>(m_stack->currentWidget()));
}

void MainWindow::refreshCurrent()
{
    if (auto *page = qobject_cast<Page *>(m_stack->currentWidget()))
        page->refresh();
}

void MainWindow::updateCartBadge()
{
    if (!m_cartButton)
        return;
    auto count = m_ui.app.cart().itemCount(m_ui.session);
    const int n = count ? count.value() : 0;
    m_cartButton->setText(n > 0 ? QStringLiteral("Cart  ·  %1").arg(n) : QStringLiteral("Cart"));
    m_cartButton->setProperty("variant", n > 0 ? QStringLiteral("primary") : QStringLiteral("topbar"));
    repolish(m_cartButton);
}

void MainWindow::openProduct(qint64 productId)
{
    ProductDetailsDialog dialog(m_ui, productId, this);
    if (dialog.isLoaded())
        dialog.exec();
}

void MainWindow::runSearch()
{
    const QString text = m_search->text().trimmed();
    if (m_ui.session.isAdmin()) {
        navigate(QStringLiteral("products"), false);
        m_pages.value(QStringLiteral("products"))->applySearch(text);
    } else {
        navigate(QStringLiteral("shop"), false);
        m_shop->applySearch(text);
    }
}

void MainWindow::toggleTheme()
{
    const ThemeMode next = Theme::current() == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark;
    Theme::apply(next);
    Theme::save(next);
    m_themeButton->setText(next == ThemeMode::Dark ? QStringLiteral("Light mode") : QStringLiteral("Dark mode"));
    refreshCurrent();
}

void MainWindow::signOut()
{
    m_ui.app.auth().logout(m_ui.session);
    emit signedOut();
    close();
}

} // namespace shophub::ui
