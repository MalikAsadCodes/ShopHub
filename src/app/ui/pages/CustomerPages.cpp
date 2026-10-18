#include "ui/pages/CustomerPages.h"

#include "services/AppContext.h"
#include "ui/dialogs/ShopDialogs.h"
#include "ui/models/TableModels.h"
#include "ui/widgets/ImageView.h"
#include "ui/widgets/ProductCard.h"
#include "ui/widgets/ProductGrid.h"
#include "ui/widgets/Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>

namespace shophub::ui {

namespace {

constexpr int kPageSize = 24;

ProductCard *createProductCard(UiContext &ui, const Product &p)
{
    auto *card = new ProductCard(p, ui.app.images().resolve(p.imageFile), ui.session.isCustomer());
    QObject::connect(card, &ProductCard::openRequested, ui.events, &AppEvents::openProduct);
    QObject::connect(card, &ProductCard::addToCartRequested, card, [&ui, name = p.name](qint64 id) {
        const Status st = ui.app.cart().add(ui.session, id, 1);
        if (!st) {
            emit ui.events->toast(st.error());
            return;
        }
        emit ui.events->cartChanged();
        emit ui.events->toast(QStringLiteral("Added %1 to your cart").arg(name));
    });
    return card;
}

QHBoxLayout *sectionHeader(const QString &title, QPushButton *action = nullptr)
{
    auto *row = new QHBoxLayout();
    row->addWidget(makeLabel(title, "h2"));
    row->addStretch();
    if (action)
        row->addWidget(action);
    return row;
}

QHBoxLayout *summaryLine(const QString &label, const QString &value, bool strong = false)
{
    auto *row = new QHBoxLayout();
    row->addWidget(makeLabel(label, strong ? "h3" : "muted"));
    row->addStretch();
    auto *v = makeLabel(value, strong ? "h2" : nullptr);
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(v);
    return row;
}

} // namespace

// ====================================================================== Home
HomePage::HomePage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_content = new QWidget();
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(28, 22, 28, 28);
    m_contentLayout->setSpacing(18);
    m_layout->addWidget(scrollWrap(m_content));
}

void HomePage::refresh()
{
    clearLayout(m_contentLayout);

    // Hero
    auto *hero = new QFrame();
    hero->setObjectName(QStringLiteral("Hero"));
    auto *hv = new QVBoxLayout(hero);
    hv->setContentsMargins(32, 28, 32, 28);
    hv->setSpacing(8);
    const QString firstName = m_ui.session.fullName().split(QLatin1Char(' ')).value(0);
    auto *title = new QLabel(QStringLiteral("Welcome back, %1").arg(firstName));
    title->setStyleSheet(QStringLiteral("font-size: 26px; font-weight: 800;"));
    hv->addWidget(title);
    auto *sub = new QLabel(QStringLiteral("Fresh picks every day. Free delivery on orders over %1, and Cash on Delivery everywhere.")
                               .arg(m_ui.app.pricing().rules().freeShippingThreshold.toString()));
    sub->setWordWrap(true);
    sub->setStyleSheet(QStringLiteral("font-size: 14px;"));
    hv->addWidget(sub);
    hv->addSpacing(8);
    auto *start = new QPushButton(QStringLiteral("Start shopping"));
    start->setCursor(Qt::PointingHandCursor);
    connect(start, &QPushButton::clicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
    hv->addWidget(start, 0, Qt::AlignLeft);
    m_contentLayout->addWidget(hero);

    // Categories
    auto cats = m_ui.app.catalog().categories(m_ui.session);
    if (cats && !cats.value().isEmpty()) {
        m_contentLayout->addLayout(sectionHeader(QStringLiteral("Shop by category")));
        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(10);
        int i = 0;
        for (const Category &c : cats.value()) {
            auto *chip = makeButton(QStringLiteral("%1   %2").arg(escapeMnemonic(c.name)).arg(c.productCount), "chip");
            connect(chip, &QPushButton::clicked, this, [this, id = c.id] { emit m_ui.events->openShop(id, QString()); });
            grid->addWidget(chip, i / 6, i % 6);
            ++i;
        }
        auto *chips = new QHBoxLayout();
        chips->addLayout(grid);
        chips->addStretch();
        m_contentLayout->addLayout(chips);
    }

    auto addSection = [this](const QString &title, ProductSort sort, int limit, bool onlySold) {
        ProductFilter f;
        f.sort = sort;
        f.limit = limit;
        auto page = m_ui.app.catalog().search(m_ui.session, f);
        if (!page)
            return;
        QVector<Product> items;
        for (const Product &p : page.value().items)
            if (!onlySold || p.unitsSold > 0)
                items.push_back(p);
        if (items.isEmpty())
            return;
        auto *seeAll = makeButton(QStringLiteral("See all"), "link");
        connect(seeAll, &QPushButton::clicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
        m_contentLayout->addSpacing(4);
        m_contentLayout->addLayout(sectionHeader(title, seeAll));
        auto *grid = new ProductGrid();
        for (const Product &p : items)
            grid->addCard(createProductCard(m_ui, p));
        m_contentLayout->addWidget(grid);
    };
    addSection(QStringLiteral("Best sellers"), ProductSort::Popularity, 4, true);
    addSection(QStringLiteral("New arrivals"), ProductSort::Newest, 8, false);

    auto all = m_ui.app.catalog().search(m_ui.session, ProductFilter());
    if (all && all.value().total == 0) {
        m_contentLayout->addWidget(new EmptyState(QStringLiteral("The shop is being stocked"),
                                                  QStringLiteral("There are no products yet. Please check back soon.")));
    }
    m_contentLayout->addStretch();
}

// ====================================================================== Shop
ShopPage::ShopPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(250);
    connect(m_debounce, &QTimer::timeout, this, [this] { runSearch(false); });

    auto *row = new QHBoxLayout();
    row->setSpacing(20);
    m_layout->addLayout(row, 1);

    // ------------------------------------------------ filters
    auto *filters = makeCard();
    filters->setFixedWidth(260);
    auto *fv = new QVBoxLayout(filters);
    fv->setContentsMargins(18, 18, 18, 18);
    fv->setSpacing(8);
    fv->addWidget(makeLabel(QStringLiteral("Filters"), "h2"));
    fv->addSpacing(4);

    m_search = new QLineEdit();
    m_search->setPlaceholderText(QStringLiteral("Name, brand or SKU"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(QStringLiteral("Search products"));
    fv->addWidget(makeLabel(QStringLiteral("Search"), "h3"));
    fv->addWidget(m_search);

    m_category = new QComboBox();
    m_category->setAccessibleName(QStringLiteral("Category"));
    fv->addWidget(makeLabel(QStringLiteral("Category"), "h3"));
    fv->addWidget(m_category);

    m_brand = new QComboBox();
    m_brand->setAccessibleName(QStringLiteral("Brand"));
    fv->addWidget(makeLabel(QStringLiteral("Brand"), "h3"));
    fv->addWidget(m_brand);

    fv->addWidget(makeLabel(QStringLiteral("Price (Rs)"), "h3"));
    auto *prices = new QHBoxLayout();
    m_minPrice = new QLineEdit();
    m_minPrice->setPlaceholderText(QStringLiteral("Min"));
    m_minPrice->setAccessibleName(QStringLiteral("Minimum price"));
    m_maxPrice = new QLineEdit();
    m_maxPrice->setPlaceholderText(QStringLiteral("Max"));
    m_maxPrice->setAccessibleName(QStringLiteral("Maximum price"));
    prices->addWidget(m_minPrice);
    prices->addWidget(makeLabel(QStringLiteral("-"), "muted"));
    prices->addWidget(m_maxPrice);
    fv->addLayout(prices);
    m_filterError = makeLabel(QString(), "error");
    m_filterError->hide();
    fv->addWidget(m_filterError);

    m_inStock = new QCheckBox(QStringLiteral("In stock only"));
    fv->addSpacing(4);
    fv->addWidget(m_inStock);

    m_sort = new QComboBox();
    m_sort->addItem(QStringLiteral("Newest first"), int(ProductSort::Newest));
    m_sort->addItem(QStringLiteral("Most popular"), int(ProductSort::Popularity));
    m_sort->addItem(QStringLiteral("Price: low to high"), int(ProductSort::PriceLowToHigh));
    m_sort->addItem(QStringLiteral("Price: high to low"), int(ProductSort::PriceHighToLow));
    m_sort->addItem(QStringLiteral("Name: A to Z"), int(ProductSort::NameAZ));
    m_sort->setAccessibleName(QStringLiteral("Sort by"));
    fv->addWidget(makeLabel(QStringLiteral("Sort by"), "h3"));
    fv->addWidget(m_sort);

    fv->addSpacing(8);
    auto *reset = makeButton(QStringLiteral("Reset filters"));
    connect(reset, &QPushButton::clicked, this, &ShopPage::resetFilters);
    fv->addWidget(reset);
    fv->addStretch();
    row->addWidget(filters, 0, Qt::AlignTop);

    // ------------------------------------------------ results
    auto *results = new QVBoxLayout();
    results->setSpacing(12);
    auto *head = new QHBoxLayout();
    head->addWidget(makeLabel(QStringLiteral("Shop"), "h1"));
    head->addStretch();
    m_count = makeLabel(QString(), "muted");
    head->addWidget(m_count, 0, Qt::AlignBottom);
    results->addLayout(head);

    m_results = new QStackedWidget();
    auto *gridHost = new QWidget();
    auto *gv = new QVBoxLayout(gridHost);
    gv->setContentsMargins(0, 0, 8, 8);
    gv->setSpacing(16);
    m_grid = new ProductGrid();
    gv->addWidget(m_grid);
    m_loadMore = makeButton(QStringLiteral("Load more products"));
    connect(m_loadMore, &QPushButton::clicked, this, [this] { runSearch(true); });
    gv->addWidget(m_loadMore, 0, Qt::AlignHCenter);
    gv->addStretch();
    m_results->addWidget(scrollWrap(gridHost));

    m_empty = new EmptyState(QStringLiteral("No products found"),
                             QStringLiteral("Try a different search, or clear some filters."),
                             QStringLiteral("Reset filters"));
    connect(m_empty, &EmptyState::actionClicked, this, &ShopPage::resetFilters);
    m_results->addWidget(m_empty);
    results->addWidget(m_results, 1);
    row->addLayout(results, 1);

    auto restart = [this] {
        if (!m_updatingFilters)
            m_debounce->start();
    };
    auto immediate = [this] {
        if (!m_updatingFilters)
            runSearch(false);
    };
    connect(m_search, &QLineEdit::textChanged, this, restart);
    connect(m_minPrice, &QLineEdit::textChanged, this, restart);
    connect(m_maxPrice, &QLineEdit::textChanged, this, restart);
    connect(m_category, qOverload<int>(&QComboBox::currentIndexChanged), this, immediate);
    connect(m_brand, qOverload<int>(&QComboBox::currentIndexChanged), this, immediate);
    connect(m_sort, qOverload<int>(&QComboBox::currentIndexChanged), this, immediate);
    connect(m_inStock, &QCheckBox::toggled, this, immediate);
}

void ShopPage::reloadFilters()
{
    m_updatingFilters = true;
    const qint64 cat = m_category->currentData().toLongLong();
    const QString brand = m_brand->currentData().toString();

    m_category->clear();
    m_category->addItem(QStringLiteral("All categories"), qint64(0));
    if (auto cats = m_ui.app.catalog().categories(m_ui.session)) {
        for (const Category &c : cats.value())
            m_category->addItem(c.name, c.id);
    }
    m_category->setCurrentIndex(qMax(0, m_category->findData(cat)));

    m_brand->clear();
    m_brand->addItem(QStringLiteral("All brands"), QString());
    if (auto brands = m_ui.app.catalog().brands(m_ui.session)) {
        for (const QString &b : brands.value())
            m_brand->addItem(b, b);
    }
    m_brand->setCurrentIndex(qMax(0, m_brand->findData(brand)));
    m_updatingFilters = false;
}

void ShopPage::refresh()
{
    reloadFilters();
    runSearch(false);
}

void ShopPage::applySearch(const QString &text)
{
    showCategory(0, text);
}

void ShopPage::showCategory(qint64 categoryId, const QString &text)
{
    reloadFilters();
    m_updatingFilters = true;
    m_category->setCurrentIndex(qMax(0, m_category->findData(categoryId)));
    m_brand->setCurrentIndex(0);
    m_search->setText(text);
    m_updatingFilters = false;
    runSearch(false);
}

void ShopPage::resetFilters()
{
    m_updatingFilters = true;
    m_search->clear();
    m_category->setCurrentIndex(0);
    m_brand->setCurrentIndex(0);
    m_minPrice->clear();
    m_maxPrice->clear();
    m_inStock->setChecked(false);
    m_sort->setCurrentIndex(0);
    m_updatingFilters = false;
    runSearch(false);
}

void ShopPage::runSearch(bool append)
{
    m_debounce->stop();
    ProductFilter f;
    f.text = m_search->text();
    f.categoryId = m_category->currentData().toLongLong();
    f.brand = m_brand->currentData().toString();
    f.inStockOnly = m_inStock->isChecked();
    f.sort = static_cast<ProductSort>(m_sort->currentData().toInt());
    f.limit = kPageSize;
    f.offset = append ? m_loaded : 0;

    QString priceError;
    if (!m_minPrice->text().trimmed().isEmpty()) {
        if (auto m = Money::parse(m_minPrice->text()))
            f.minPrice = *m;
        else
            priceError = QStringLiteral("Enter prices as numbers, e.g. 1500");
    }
    if (!m_maxPrice->text().trimmed().isEmpty()) {
        if (auto m = Money::parse(m_maxPrice->text()))
            f.maxPrice = *m;
        else
            priceError = QStringLiteral("Enter prices as numbers, e.g. 1500");
    }
    m_filterError->setText(priceError);
    m_filterError->setVisible(!priceError.isEmpty());

    auto page = m_ui.app.catalog().search(m_ui.session, f);
    if (!page) {
        emit m_ui.events->toast(page.error());
        return;
    }
    if (!append) {
        m_grid->clear();
        m_loaded = 0;
    }
    for (const Product &p : page.value().items)
        m_grid->addCard(createProductCard(m_ui, p));
    m_loaded += page.value().items.size();

    const int total = page.value().total;
    m_count->setText(total == 0 ? QString() : QStringLiteral("Showing %1 of %2 products").arg(m_loaded).arg(total));
    m_loadMore->setVisible(m_loaded < total);
    m_results->setCurrentIndex(total == 0 ? 1 : 0);
}

// ====================================================================== Cart
CartPage::CartPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    addHeader(QStringLiteral("Your cart"), QStringLiteral("Review your items before checking out."));

    m_stack = new QStackedWidget();
    auto *empty = new EmptyState(QStringLiteral("Your cart is empty"),
                                 QStringLiteral("Browse the shop and add something you like."),
                                 QStringLiteral("Start shopping"));
    connect(empty, &EmptyState::actionClicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
    m_stack->addWidget(empty);

    auto *content = new QWidget();
    auto *row = new QHBoxLayout(content);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(20);

    m_rowsHost = new QWidget();
    m_rows = new QVBoxLayout(m_rowsHost);
    m_rows->setContentsMargins(0, 0, 8, 0);
    m_rows->setSpacing(10);
    row->addWidget(scrollWrap(m_rowsHost), 1);

    auto *summaryCard = makeCard();
    summaryCard->setFixedWidth(330);
    m_summary = new QVBoxLayout(summaryCard);
    m_summary->setContentsMargins(20, 18, 20, 18);
    m_summary->setSpacing(10);
    row->addWidget(summaryCard, 0, Qt::AlignTop);

    m_stack->addWidget(content);
    m_layout->addWidget(m_stack, 1);
}

QWidget *CartPage::buildRow(const CartItem &item)
{
    auto *card = makeCard();
    auto *h = new QHBoxLayout(card);
    h->setContentsMargins(14, 12, 16, 12);
    h->setSpacing(14);

    auto *image = new ImageView();
    image->setFixedSize(72, 72);
    image->setImage(m_ui.app.images().resolve(item.imageFile), item.name, item.categoryName);
    h->addWidget(image);

    auto *info = new QVBoxLayout();
    info->setSpacing(3);
    auto *name = makeButton(escapeMnemonic(item.name), "link");
    name->setStyleSheet(QStringLiteral("font-size: 14px; text-align: left;"));
    connect(name, &QPushButton::clicked, this, [this, id = item.productId] { emit m_ui.events->openProduct(id); });
    info->addWidget(name, 0, Qt::AlignLeft);
    info->addWidget(makeLabel(QStringLiteral("%1  ·  %2").arg(item.sku, item.categoryName), "small"));
    info->addWidget(makeLabel(QStringLiteral("%1 each").arg(item.unitPrice.toString()), "muted"));
    const QStringList problems = CartService::problems({item});
    if (!problems.isEmpty()) {
        auto *warn = makeLabel(problems.first(), "error");
        warn->setWordWrap(true);
        info->addWidget(warn);
    }
    h->addLayout(info, 1);

    auto *qty = new QSpinBox();
    qty->setRange(1, CartService::kMaxPerLine);
    qty->setValue(item.quantity);
    qty->setKeyboardTracking(false); // only react on Enter / arrows / focus-out
    qty->setMinimumHeight(34);
    qty->setMinimumWidth(70);
    qty->setAccessibleName(QStringLiteral("Quantity of %1").arg(item.name));
    connect(qty, qOverload<int>(&QSpinBox::valueChanged), this, [this, id = item.productId](int value) {
        const Status st = m_ui.app.cart().setQuantity(m_ui.session, id, value);
        if (!st)
            emit m_ui.events->toast(st.error());
        // Refresh after the spin box has finished handling its own event.
        QTimer::singleShot(0, m_ui.events, &AppEvents::cartChanged);
    });
    h->addWidget(qty);

    auto *total = makeLabel(item.lineTotal().toString(), "h3");
    total->setMinimumWidth(120);
    total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(total);

    auto *remove = makeButton(QStringLiteral("Remove"), "ghost");
    remove->setAccessibleName(QStringLiteral("Remove %1").arg(item.name));
    connect(remove, &QPushButton::clicked, this, [this, id = item.productId, name = item.name] {
        const Status st = m_ui.app.cart().remove(m_ui.session, id);
        if (!st) {
            emit m_ui.events->toast(st.error());
            return;
        }
        emit m_ui.events->toast(QStringLiteral("Removed %1").arg(name));
        QTimer::singleShot(0, m_ui.events, &AppEvents::cartChanged);
    });
    h->addWidget(remove);
    return card;
}

void CartPage::refresh()
{
    auto items = m_ui.app.cart().items(m_ui.session);
    if (!items) {
        emit m_ui.events->toast(items.error());
        return;
    }
    clearLayout(m_rows);
    clearLayout(m_summary);
    if (items.value().isEmpty()) {
        m_stack->setCurrentIndex(0);
        return;
    }
    m_stack->setCurrentIndex(1);

    for (const CartItem &item : items.value())
        m_rows->addWidget(buildRow(item));
    m_rows->addStretch();

    const PriceBreakdown price = m_ui.app.pricing().price(items.value());
    m_summary->addWidget(makeLabel(QStringLiteral("Order summary"), "h2"));
    m_summary->addLayout(summaryLine(QStringLiteral("Items (%1)").arg(price.itemCount), price.subtotal.toString()));
    m_summary->addLayout(summaryLine(QStringLiteral("Delivery"),
                                     price.shipping.isZero() ? QStringLiteral("Free") : price.shipping.toString()));
    if (!price.tax.isZero())
        m_summary->addLayout(summaryLine(QStringLiteral("Tax"), price.tax.toString()));
    m_summary->addWidget(makeDivider());
    m_summary->addLayout(summaryLine(QStringLiteral("Total"), price.total.toString(), true));

    const Money threshold = m_ui.app.pricing().rules().freeShippingThreshold;
    if (!price.shipping.isZero() && price.subtotal < threshold)
        m_summary->addWidget(makeBadge(QStringLiteral("Add %1 more for free delivery").arg((threshold - price.subtotal).toString()), QStringLiteral("info")));
    else if (price.shipping.isZero())
        m_summary->addWidget(makeBadge(QStringLiteral("You get free delivery"), QStringLiteral("success")));

    const QStringList problems = CartService::problems(items.value());
    if (!problems.isEmpty()) {
        auto *warn = makeLabel(QStringLiteral("Please fix the items marked in red before checking out."), "error");
        warn->setWordWrap(true);
        m_summary->addWidget(warn);
    }

    m_summary->addSpacing(6);
    auto *checkout = makeButton(QStringLiteral("Proceed to checkout"), "primary");
    checkout->setMinimumHeight(44);
    checkout->setEnabled(problems.isEmpty());
    connect(checkout, &QPushButton::clicked, this, &CartPage::openCheckout);
    m_summary->addWidget(checkout);

    auto *more = makeButton(QStringLiteral("Continue shopping"), "ghost");
    connect(more, &QPushButton::clicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
    m_summary->addWidget(more);

    auto *clear = makeButton(QStringLiteral("Empty cart"), "link");
    connect(clear, &QPushButton::clicked, this, [this] {
        if (!confirm(this, QStringLiteral("Empty cart"), QStringLiteral("Remove every item from your cart?"),
                     QStringLiteral("Empty cart")))
            return;
        m_ui.app.cart().clear(m_ui.session);
        emit m_ui.events->cartChanged();
    });
    m_summary->addWidget(clear, 0, Qt::AlignHCenter);
    m_summary->addWidget(makeLabel(QStringLiteral("Payment: Cash on Delivery"), "small"), 0, Qt::AlignHCenter);
}

void CartPage::openCheckout()
{
    CheckoutDialog dialog(m_ui, window());
    if (dialog.exec() != QDialog::Accepted)
        return;
    const Order &o = dialog.placedOrder();
    showInfo(window(), QStringLiteral("Order placed"),
             QStringLiteral("Thank you! Your order %1 has been placed.\n\n"
                            "Total: %2, to be paid in cash on delivery.\n"
                            "You can follow it under My orders.")
                 .arg(o.orderNumber, o.total.toString()));
    emit m_ui.events->navigate(QStringLiteral("orders"));
}

// ====================================================================== My orders
MyOrdersPage::MyOrdersPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    auto *details = makeButton(QStringLiteral("View details"), "primary");
    addHeader(QStringLiteral("My orders"), QStringLiteral("Track your orders and cancel them before they ship."), details);

    m_stack = new QStackedWidget();
    auto *empty = new EmptyState(QStringLiteral("No orders yet"),
                                 QStringLiteral("When you place an order, it will show up here."),
                                 QStringLiteral("Start shopping"));
    connect(empty, &EmptyState::actionClicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
    m_stack->addWidget(empty);

    m_model = new OrderTableModel(OrderTableModel::Mode::Customer, this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(kSortRole);
    m_table = new QTableView();
    m_table->setModel(m_proxy);
    setupTable(m_table);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->sortByColumn(1, Qt::DescendingOrder);
    m_stack->addWidget(m_table);
    m_layout->addWidget(m_stack, 1);
    m_layout->addWidget(makeLabel(QStringLiteral("Double-click an order to see its items and status history."), "small"));

    connect(details, &QPushButton::clicked, this, &MyOrdersPage::openSelected);
    connect(m_table, &QTableView::doubleClicked, this, &MyOrdersPage::openSelected);
}

void MyOrdersPage::refresh()
{
    auto orders = m_ui.app.orders().myOrders(m_ui.session);
    if (!orders) {
        emit m_ui.events->toast(orders.error());
        return;
    }
    m_model->setOrders(orders.value());
    m_stack->setCurrentIndex(orders.value().isEmpty() ? 0 : 1);
    if (!orders.value().isEmpty() && !m_table->currentIndex().isValid())
        m_table->selectRow(0);
}

void MyOrdersPage::openSelected()
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid())
        return;
    const qint64 id = m_proxy->data(index, kIdRole).toLongLong();
    OrderDetailsDialog dialog(m_ui, id, window());
    if (dialog.isLoaded())
        dialog.exec();
    refresh();
}

} // namespace shophub::ui
