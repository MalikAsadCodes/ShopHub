#include "ui/pages/AdminPages.h"

#include "services/AppContext.h"
#include "ui/Theme.h"
#include "ui/dialogs/AdminDialogs.h"
#include "ui/dialogs/ShopDialogs.h"
#include "ui/models/TableModels.h"
#include "ui/widgets/Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTableWidget>
#include <QTimer>

namespace shophub::ui {

namespace {

QLineEdit *searchBox(const QString &placeholder)
{
    auto *e = new QLineEdit();
    e->setPlaceholderText(placeholder);
    e->setClearButtonEnabled(true);
    e->setMinimumWidth(280);
    e->setAccessibleName(placeholder);
    return e;
}

// Re-selects the row that was selected before a reload, or the first row.
void selectRowWithId(QTableView *table, QSortFilterProxyModel *proxy, qint64 id)
{
    int row = proxy->rowCount() > 0 ? 0 : -1;
    for (int r = 0; r < proxy->rowCount(); ++r) {
        if (proxy->data(proxy->index(r, 0), kIdRole).toLongLong() == id) {
            row = r;
            break;
        }
    }
    if (row >= 0)
        table->selectRow(row);
}

QTimer *debounceTimer(QObject *owner)
{
    auto *t = new QTimer(owner);
    t->setSingleShot(true);
    t->setInterval(250);
    return t;
}

} // namespace

// ====================================================================== Dashboard
DashboardPage::DashboardPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    auto *content = new QWidget();
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(28, 22, 28, 28);
    v->setSpacing(18);

    auto *refreshBtn = makeButton(QStringLiteral("Refresh"));
    connect(refreshBtn, &QPushButton::clicked, this, &DashboardPage::refresh);
    auto *head = new QHBoxLayout();
    auto *texts = new QVBoxLayout();
    texts->setSpacing(2);
    texts->addWidget(makeLabel(QStringLiteral("Dashboard"), "h1"));
    texts->addWidget(makeLabel(QStringLiteral("Live figures from your orders. Cancelled orders are excluded; "
                                              "totals include delivery and tax."),
                               "muted"));
    head->addLayout(texts);
    head->addStretch();
    head->addWidget(refreshBtn, 0, Qt::AlignBottom);
    v->addLayout(head);

    auto *stats = new QGridLayout();
    stats->setHorizontalSpacing(14);
    stats->setVerticalSpacing(14);
    m_sales = new StatCard(QStringLiteral("Gross sales"));
    m_orders = new StatCard(QStringLiteral("Orders"));
    m_khata = new StatCard(QStringLiteral("Khata outstanding"));
    m_open = new StatCard(QStringLiteral("Orders to fulfil"));
    m_lowStock = new StatCard(QStringLiteral("Low-stock products"));
    m_customers = new StatCard(QStringLiteral("Customers"));
    const QList<StatCard *> cards = {m_sales, m_orders, m_khata, m_open, m_lowStock, m_customers};
    for (int i = 0; i < cards.size(); ++i)
        stats->addWidget(cards[i], i / 3, i % 3);
    v->addLayout(stats);

    auto *chartCard = makeCard();
    auto *cv = new QVBoxLayout(chartCard);
    cv->setContentsMargins(20, 16, 20, 16);
    auto *chartHead = new QHBoxLayout();
    chartHead->addWidget(makeLabel(QStringLiteral("Sales, last 14 days"), "h2"));
    chartHead->addStretch();
    m_chartTotal = makeLabel(QString(), "muted");
    chartHead->addWidget(m_chartTotal);
    cv->addLayout(chartHead);
    m_chart = new SalesChart();
    cv->addWidget(m_chart);
    v->addWidget(chartCard);

    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(14);

    auto *topCard = makeCard();
    topCard->setMinimumWidth(340);
    auto *tv = new QVBoxLayout(topCard);
    tv->setContentsMargins(20, 16, 20, 16);
    tv->setSpacing(10);
    tv->addWidget(makeLabel(QStringLiteral("Top products"), "h2"));
    m_topProducts = new QVBoxLayout();
    m_topProducts->setSpacing(8);
    tv->addLayout(m_topProducts);
    tv->addStretch();
    bottom->addWidget(topCard, 2);

    auto *recentCard = makeCard();
    auto *rv = new QVBoxLayout(recentCard);
    rv->setContentsMargins(20, 16, 20, 16);
    rv->setSpacing(10);
    auto *recentHead = new QHBoxLayout();
    recentHead->addWidget(makeLabel(QStringLiteral("Recent orders"), "h2"));
    recentHead->addStretch();
    auto *all = makeButton(QStringLiteral("All orders"), "link");
    connect(all, &QPushButton::clicked, this, [this] { emit m_ui.events->navigate(QStringLiteral("admin-orders")); });
    recentHead->addWidget(all);
    rv->addLayout(recentHead);
    m_recent = new QTableWidget(0, 4);
    m_recent->setHorizontalHeaderLabels({QStringLiteral("Order #"), QStringLiteral("Customer"), QStringLiteral("Total"),
                                         QStringLiteral("Status")});
    m_recent->verticalHeader()->hide();
    m_recent->verticalHeader()->setDefaultSectionSize(34);
    m_recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recent->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recent->setShowGrid(false);
    m_recent->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_recent->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_recent->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_recent->setFocusPolicy(Qt::NoFocus);
    m_recent->setMinimumHeight(300);
    connect(m_recent, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0 || row >= m_recentIds.size())
            return;
        OrderDetailsDialog dialog(m_ui, m_recentIds[row], window());
        if (dialog.isLoaded())
            dialog.exec();
        refresh();
    });
    rv->addWidget(m_recent);
    rv->addWidget(makeLabel(QStringLiteral("Double-click an order to open it."), "small"));
    bottom->addWidget(recentCard, 3);
    v->addLayout(bottom);
    v->addStretch();

    m_layout->addWidget(scrollWrap(content));
}

void DashboardPage::refresh()
{
    auto result = m_ui.app.dashboard().stats(m_ui.session);
    if (!result) {
        emit m_ui.events->toast(result.error());
        return;
    }
    const DashboardStats &s = result.value();
    m_sales->setValue(s.grossSales.toString(), QStringLiteral("All time"));
    m_orders->setValue(QString::number(s.orderCount),
                       QStringLiteral("Not cancelled  ·  average %1").arg(s.averageOrderValue.toString()));
    m_khata->setValue(s.khataOutstanding.toString(),
                      s.khataDebtors > 0 ? QStringLiteral("Owed by %1 customer(s) - see Khata").arg(s.khataDebtors)
                                         : QStringLiteral("Nobody owes the shop"));
    m_open->setValue(QString::number(s.pendingOrders),
                     s.pendingOrders > 0 ? QStringLiteral("Pending, confirmed or processing") : QStringLiteral("All caught up"),
                     s.pendingOrders > 0 ? QStringLiteral("small") : QStringLiteral("small"));
    m_lowStock->setValue(QString::number(s.lowStockCount),
                         s.lowStockCount > 0 ? QStringLiteral("Restock soon - see Inventory") : QStringLiteral("Stock levels look healthy"),
                         s.lowStockCount > 0 ? QStringLiteral("error") : QStringLiteral("small"));
    m_customers->setValue(QString::number(s.customerCount), QStringLiteral("Registered accounts"));

    m_chart->setData(s.last14Days);
    Money twoWeeks;
    int twoWeekOrders = 0;
    for (const DailySales &d : s.last14Days) {
        twoWeeks += d.revenue;
        twoWeekOrders += d.orders;
    }
    m_chartTotal->setText(QStringLiteral("%1 from %2 order(s)").arg(twoWeeks.toString()).arg(twoWeekOrders));

    clearLayout(m_topProducts);
    if (s.topProducts.isEmpty())
        m_topProducts->addWidget(makeLabel(QStringLiteral("No sales yet."), "muted"));
    int rank = 1;
    for (const TopProduct &t : s.topProducts) {
        auto *row = new QHBoxLayout();
        auto *num = makeLabel(QString::number(rank++), "h3");
        num->setFixedWidth(22);
        row->addWidget(num);
        auto *name = makeLabel(t.name);
        name->setWordWrap(true);
        row->addWidget(name, 1);
        auto *units = makeBadge(QStringLiteral("%1 sold").arg(t.units), QStringLiteral("accent"));
        row->addWidget(units);
        auto *rev = makeLabel(t.revenue.toString(), "muted");
        rev->setMinimumWidth(100);
        rev->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(rev);
        m_topProducts->addLayout(row);
    }

    m_recent->setRowCount(s.recentOrders.size());
    m_recentIds.clear();
    for (int r = 0; r < s.recentOrders.size(); ++r) {
        const Order &o = s.recentOrders[r];
        m_recentIds.push_back(o.id);
        auto *num = new QTableWidgetItem(o.orderNumber);
        auto *who = new QTableWidgetItem(o.customerName);
        auto *total = new QTableWidgetItem(o.total.toString());
        auto *status = new QTableWidgetItem(orderstatus::displayName(o.status));
        status->setForeground(Theme::statusColor(o.status));
        QFont bold = status->font();
        bold.setBold(true);
        status->setFont(bold);
        num->setFont(bold);
        m_recent->setItem(r, 0, num);
        m_recent->setItem(r, 1, who);
        m_recent->setItem(r, 2, total);
        m_recent->setItem(r, 3, status);
    }
}

// ====================================================================== Products
AdminProductsPage::AdminProductsPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    auto *add = makeButton(QStringLiteral("New product"), "primary");
    addHeader(QStringLiteral("Products"), QStringLiteral("Add, edit and hide products in your catalog."), add);
    connect(add, &QPushButton::clicked, this, [this] {
        ProductEditDialog dialog(m_ui, std::nullopt, window());
        dialog.exec();
    });

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);
    m_search = searchBox(QStringLiteral("Search by name, SKU or brand"));
    m_category = new QComboBox();
    m_category->setMinimumWidth(180);
    m_showInactive = new QCheckBox(QStringLiteral("Show hidden products"));
    m_showInactive->setChecked(true);
    toolbar->addWidget(m_search);
    toolbar->addWidget(m_category);
    toolbar->addWidget(m_showInactive);
    toolbar->addStretch();
    auto *categories = makeButton(QStringLiteral("Categories..."));
    connect(categories, &QPushButton::clicked, this, [this] {
        CategoriesDialog dialog(m_ui, window());
        dialog.exec();
        refresh();
    });
    toolbar->addWidget(categories);
    m_layout->addLayout(toolbar);

    m_model = new ProductTableModel({ProductTableModel::Sku, ProductTableModel::Name, ProductTableModel::Category,
                                     ProductTableModel::Brand, ProductTableModel::Price, ProductTableModel::Cost,
                                     ProductTableModel::Stock, ProductTableModel::Wholesale, ProductTableModel::Status},
                                    this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(kSortRole);
    m_table = new QTableView();
    m_table->setModel(m_proxy);
    setupTable(m_table);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->sortByColumn(1, Qt::AscendingOrder);
    m_layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout();
    m_count = makeLabel(QString(), "muted");
    actions->addWidget(m_count);
    actions->addStretch();
    m_preview = makeButton(QStringLiteral("Preview"));
    m_toggle = makeButton(QStringLiteral("Hide from shop"));
    m_tiers = makeButton(QStringLiteral("Quantity prices..."));
    m_tiers->setToolTip(QStringLiteral("Wholesale prices: a lower price when a customer buys more, e.g. 12 or more at 5% off"));
    m_edit = makeButton(QStringLiteral("Edit..."), "primary");
    actions->addWidget(m_preview);
    actions->addWidget(m_toggle);
    actions->addWidget(m_tiers);
    actions->addWidget(m_edit);
    m_layout->addLayout(actions);

    m_debounce = debounceTimer(this);
    connect(m_debounce, &QTimer::timeout, this, &AdminProductsPage::refresh);
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_category, qOverload<int>(&QComboBox::activated), this, &AdminProductsPage::refresh);
    connect(m_showInactive, &QCheckBox::toggled, this, &AdminProductsPage::refresh);
    connect(m_table, &QTableView::doubleClicked, this, &AdminProductsPage::editSelected);
    connect(m_edit, &QPushButton::clicked, this, &AdminProductsPage::editSelected);
    connect(m_toggle, &QPushButton::clicked, this, &AdminProductsPage::toggleSelected);
    connect(m_tiers, &QPushButton::clicked, this, &AdminProductsPage::editTiersOfSelected);
    connect(m_preview, &QPushButton::clicked, this, [this] {
        if (const Product *p = selected())
            emit m_ui.events->openProduct(p->id);
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &AdminProductsPage::updateButtons);
}

void AdminProductsPage::applySearch(const QString &text)
{
    m_search->setText(text);
    refresh();
}

void AdminProductsPage::refresh()
{
    m_debounce->stop();
    const qint64 keepCategory = m_category->currentData().toLongLong();
    m_category->blockSignals(true);
    m_category->clear();
    m_category->addItem(QStringLiteral("All categories"), qint64(0));
    if (auto cats = m_ui.app.catalog().categories(m_ui.session, true))
        for (const Category &c : cats.value())
            m_category->addItem(c.active ? c.name : c.name + QStringLiteral(" (inactive)"), c.id);
    m_category->setCurrentIndex(qMax(0, m_category->findData(keepCategory)));
    m_category->blockSignals(false);

    const Product *current = selected();
    const qint64 keepId = current ? current->id : 0;

    ProductFilter f;
    f.text = m_search->text();
    f.categoryId = m_category->currentData().toLongLong();
    f.includeInactive = m_showInactive->isChecked();
    f.sort = ProductSort::NameAZ;
    f.limit = 500;
    auto page = m_ui.app.catalog().search(m_ui.session, f);
    if (!page) {
        emit m_ui.events->toast(page.error());
        return;
    }
    m_model->setProducts(page.value().items);
    m_count->setText(QStringLiteral("%1 product(s)").arg(page.value().total));

    selectRowWithId(m_table, m_proxy, keepId);
    updateButtons();
}

const Product *AdminProductsPage::selected() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return m_model->productAt(m_proxy->mapToSource(idx).row());
}

void AdminProductsPage::updateButtons()
{
    const Product *p = selected();
    m_edit->setEnabled(p);
    m_toggle->setEnabled(p);
    m_preview->setEnabled(p);
    m_tiers->setEnabled(p);
    m_toggle->setText(p && !p->active ? QStringLiteral("Show in shop") : QStringLiteral("Hide from shop"));
}

void AdminProductsPage::editSelected()
{
    const Product *p = selected();
    if (!p)
        return;
    ProductEditDialog dialog(m_ui, p->id, window());
    if (dialog.isLoaded())
        dialog.exec();
}

void AdminProductsPage::editTiersOfSelected()
{
    const Product *p = selected();
    if (!p)
        return;
    PriceTiersDialog dialog(m_ui, *p, window());
    if (dialog.isLoaded() && dialog.exec() == QDialog::Accepted)
        refresh();
}

void AdminProductsPage::toggleSelected()
{
    const Product *p = selected();
    if (!p)
        return;
    const bool activate = !p->active;
    if (!activate && !confirm(this, QStringLiteral("Hide product"),
                              QStringLiteral("Hide \"%1\" from the shop? Customers will no longer see it. "
                                             "Existing orders are not affected.")
                                  .arg(p->name),
                              QStringLiteral("Hide product")))
        return;
    const QString name = p->name;
    const Status st = m_ui.app.catalog().setProductActive(m_ui.session, p->id, activate);
    if (!st) {
        showError(this, st.error());
        return;
    }
    emit m_ui.events->catalogChanged();
    emit m_ui.events->toast(activate ? QStringLiteral("%1 is visible in the shop again").arg(name)
                                     : QStringLiteral("%1 is now hidden from customers").arg(name));
}

// ====================================================================== Inventory
InventoryPage::InventoryPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    m_adjust = makeButton(QStringLiteral("Adjust stock..."), "primary");
    addHeader(QStringLiteral("Inventory"), QStringLiteral("Every stock change is recorded with a reason and who made it."), m_adjust);

    auto *toolbar = new QHBoxLayout();
    m_search = searchBox(QStringLiteral("Search by name, SKU or brand"));
    m_lowOnly = new QCheckBox(QStringLiteral("Low or out of stock only"));
    toolbar->addWidget(m_search);
    toolbar->addWidget(m_lowOnly);
    toolbar->addStretch();
    m_summary = makeLabel(QString(), "muted");
    toolbar->addWidget(m_summary);
    m_layout->addLayout(toolbar);

    m_model = new ProductTableModel({ProductTableModel::Sku, ProductTableModel::Name, ProductTableModel::Category,
                                     ProductTableModel::Stock, ProductTableModel::LowAt, ProductTableModel::StockState},
                                    this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(kSortRole);
    m_table = new QTableView();
    m_table->setModel(m_proxy);
    setupTable(m_table);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->sortByColumn(5, Qt::AscendingOrder); // most urgent first
    m_layout->addWidget(m_table, 3);

    m_movementsTitle = makeLabel(QStringLiteral("Recent stock movements"), "h2");
    m_layout->addWidget(m_movementsTitle);
    m_movements = new MovementTableModel(this);
    m_movementTable = new QTableView();
    m_movementTable->setModel(m_movements);
    setupTable(m_movementTable);
    m_movementTable->setSortingEnabled(false);
    m_movementTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_movementTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_movementTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_layout->addWidget(m_movementTable, 2);

    m_debounce = debounceTimer(this);
    connect(m_debounce, &QTimer::timeout, this, &InventoryPage::refresh);
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_lowOnly, &QCheckBox::toggled, this, &InventoryPage::refresh);
    connect(m_adjust, &QPushButton::clicked, this, &InventoryPage::adjustSelected);
    connect(m_table, &QTableView::doubleClicked, this, &InventoryPage::adjustSelected);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &InventoryPage::loadMovements);
}

void InventoryPage::applySearch(const QString &text)
{
    m_search->setText(text);
    refresh();
}

const Product *InventoryPage::selected() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return m_model->productAt(m_proxy->mapToSource(idx).row());
}

void InventoryPage::refresh()
{
    m_debounce->stop();
    const Product *current = selected();
    const qint64 keepId = current ? current->id : 0;

    ProductFilter f;
    f.text = m_search->text();
    f.includeInactive = true;
    f.sort = ProductSort::NameAZ;
    f.limit = 500;
    auto page = m_ui.app.catalog().search(m_ui.session, f);
    if (!page) {
        emit m_ui.events->toast(page.error());
        return;
    }
    QVector<Product> rows;
    int low = 0;
    int out = 0;
    for (const Product &p : page.value().items) {
        if (p.stock <= 0)
            ++out;
        else if (p.isLowStock())
            ++low;
        if (!m_lowOnly->isChecked() || p.isLowStock())
            rows.push_back(p);
    }
    m_model->setProducts(rows);
    m_summary->setText(QStringLiteral("%1 products  ·  %2 low  ·  %3 out of stock")
                           .arg(page.value().items.size()).arg(low).arg(out));

    selectRowWithId(m_table, m_proxy, keepId);
    loadMovements();
}

void InventoryPage::loadMovements()
{
    const Product *p = selected();
    m_adjust->setEnabled(p);
    m_movementsTitle->setText(p ? QStringLiteral("Stock movements - %1").arg(p->name)
                                : QStringLiteral("Recent stock movements (all products)"));
    auto moves = m_ui.app.inventory().movements(m_ui.session, p ? p->id : 0, 200);
    if (moves)
        m_movements->setMovements(moves.value());
}

void InventoryPage::adjustSelected()
{
    const Product *p = selected();
    if (!p)
        return;
    StockAdjustDialog dialog(m_ui, *p, window());
    dialog.exec();
}

// ====================================================================== Orders (admin)
AdminOrdersPage::AdminOrdersPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    addHeader(QStringLiteral("Orders"), QStringLiteral("Move orders through fulfilment, or cancel them before they ship."));

    auto *toolbar = new QHBoxLayout();
    m_status = new QComboBox();
    m_status->addItem(QStringLiteral("All statuses"), QString());
    for (OrderStatus s : orderstatus::all())
        m_status->addItem(orderstatus::displayName(s), orderstatus::toDb(s));
    m_status->setMinimumWidth(160);
    m_search = searchBox(QStringLiteral("Order number, customer name or email"));
    toolbar->addWidget(m_status);
    toolbar->addWidget(m_search);
    toolbar->addStretch();
    m_count = makeLabel(QString(), "muted");
    toolbar->addWidget(m_count);
    m_layout->addLayout(toolbar);

    m_model = new OrderTableModel(OrderTableModel::Mode::Admin, this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(kSortRole);
    m_table = new QTableView();
    m_table->setModel(m_proxy);
    setupTable(m_table);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->sortByColumn(1, Qt::DescendingOrder);
    m_layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout();
    actions->addWidget(makeLabel(QStringLiteral("Double-click an order to see everything about it."), "small"));
    actions->addStretch();
    m_cancel = makeButton(QStringLiteral("Cancel order"), "danger");
    m_view = makeButton(QStringLiteral("View details"));
    m_advance = makeButton(QStringLiteral("Advance"), "primary");
    actions->addWidget(m_cancel);
    actions->addWidget(m_view);
    actions->addWidget(m_advance);
    m_layout->addLayout(actions);

    m_debounce = debounceTimer(this);
    connect(m_debounce, &QTimer::timeout, this, &AdminOrdersPage::refresh);
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_status, qOverload<int>(&QComboBox::activated), this, &AdminOrdersPage::refresh);
    connect(m_table, &QTableView::doubleClicked, this, &AdminOrdersPage::openSelected);
    connect(m_view, &QPushButton::clicked, this, &AdminOrdersPage::openSelected);
    connect(m_advance, &QPushButton::clicked, this, &AdminOrdersPage::advanceSelected);
    connect(m_cancel, &QPushButton::clicked, this, &AdminOrdersPage::cancelSelected);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &AdminOrdersPage::updateButtons);
}

void AdminOrdersPage::applySearch(const QString &text)
{
    m_search->setText(text);
    refresh();
}

const Order *AdminOrdersPage::selected() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return m_model->orderAt(m_proxy->mapToSource(idx).row());
}

void AdminOrdersPage::refresh()
{
    m_debounce->stop();
    const Order *current = selected();
    const qint64 keepId = current ? current->id : 0;

    OrderQuery q;
    q.search = m_search->text();
    if (const QString s = m_status->currentData().toString(); !s.isEmpty())
        q.status = orderstatus::fromDb(s);
    auto orders = m_ui.app.orders().allOrders(m_ui.session, q);
    if (!orders) {
        emit m_ui.events->toast(orders.error());
        return;
    }
    m_model->setOrders(orders.value());
    m_count->setText(QStringLiteral("%1 order(s)").arg(orders.value().size()));
    selectRowWithId(m_table, m_proxy, keepId);
    updateButtons();
}

void AdminOrdersPage::updateButtons()
{
    const Order *o = selected();
    m_view->setEnabled(o);
    const auto next = o ? orderstatus::nextForward(o->status) : std::nullopt;
    m_advance->setEnabled(next.has_value());
    m_advance->setText(next ? QStringLiteral("Mark as %1").arg(orderstatus::displayName(*next)) : QStringLiteral("Advance"));
    m_cancel->setEnabled(o && orderstatus::adminCanCancel(o->status));
}

void AdminOrdersPage::openSelected()
{
    const Order *o = selected();
    if (!o)
        return;
    OrderDetailsDialog dialog(m_ui, o->id, window());
    if (dialog.isLoaded())
        dialog.exec();
}

void AdminOrdersPage::advanceSelected()
{
    const Order *o = selected();
    if (!o)
        return;
    const auto next = orderstatus::nextForward(o->status);
    if (!next)
        return;
    const QString number = o->orderNumber;
    const Status st = m_ui.app.orders().advance(m_ui.session, o->id, *next);
    if (!st) {
        showError(this, st.error());
        return;
    }
    emit m_ui.events->ordersChanged();
    emit m_ui.events->toast(QStringLiteral("%1 marked as %2").arg(number, orderstatus::displayName(*next)));
}

void AdminOrdersPage::cancelSelected()
{
    const Order *o = selected();
    if (!o)
        return;
    const QString number = o->orderNumber;
    const qint64 id = o->id;
    if (!confirm(this, QStringLiteral("Cancel order"),
                 QStringLiteral("Cancel order %1? Its items will go back into stock.").arg(number),
                 QStringLiteral("Cancel order")))
        return;
    const Status st = m_ui.app.orders().cancel(m_ui.session, id, QString());
    if (!st) {
        showError(this, st.error());
        return;
    }
    emit m_ui.events->ordersChanged();
    emit m_ui.events->catalogChanged();
    emit m_ui.events->toast(QStringLiteral("Order %1 cancelled").arg(number));
}

} // namespace shophub::ui
