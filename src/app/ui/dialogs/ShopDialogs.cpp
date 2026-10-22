#include "ui/dialogs/ShopDialogs.h"

#include "services/AppContext.h"
#include "ui/Theme.h"
#include "ui/widgets/ImageView.h"

#include <QApplication>
#include <QCheckBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace shophub::ui {

namespace {

QHBoxLayout *summaryRow(const QString &label, const QString &value, bool strong = false)
{
    auto *row = new QHBoxLayout();
    auto *l = makeLabel(label, strong ? "h3" : "muted");
    auto *v = makeLabel(value, strong ? "h3" : nullptr);
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(l);
    row->addStretch();
    row->addWidget(v);
    return row;
}

QLineEdit *formField(const QString &placeholder)
{
    auto *e = new QLineEdit();
    e->setPlaceholderText(placeholder);
    e->setAccessibleName(placeholder);
    e->setMinimumHeight(36);
    return e;
}

const QStringList kCities = {
    QStringLiteral("Karachi"), QStringLiteral("Lahore"), QStringLiteral("Islamabad"), QStringLiteral("Rawalpindi"),
    QStringLiteral("Faisalabad"), QStringLiteral("Multan"), QStringLiteral("Peshawar"), QStringLiteral("Quetta"),
    QStringLiteral("Sialkot"), QStringLiteral("Gujranwala"), QStringLiteral("Hyderabad"), QStringLiteral("Bahawalpur"),
    QStringLiteral("Sargodha"), QStringLiteral("Abbottabad"), QStringLiteral("Attock"), QStringLiteral("Taunsa")};

} // namespace

// ====================================================================== Product details
ProductDetailsDialog::ProductDetailsDialog(UiContext &ui, qint64 productId, QWidget *parent)
    : QDialog(parent), m_ui(ui)
{
    setWindowTitle(QStringLiteral("Product details"));
    auto result = ui.app.catalog().product(ui.session, productId);
    if (!result) {
        showError(parent, result.error());
        return;
    }
    m_product = result.value();
    m_loaded = true;
    setMinimumWidth(780);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(28);

    auto *image = new ImageView(this);
    image->setFixedSize(340, 320);
    image->setRadius(14);
    image->setImage(ui.app.images().resolve(m_product.imageFile), m_product.name, m_product.categoryName);
    layout->addWidget(image, 0, Qt::AlignTop);

    auto *info = new QVBoxLayout();
    info->setSpacing(8);
    info->addWidget(makeLabel(m_product.categoryName.toUpper(), "small"));
    auto *name = makeLabel(m_product.name, "h1");
    name->setWordWrap(true);
    info->addWidget(name);
    if (!m_product.brand.isEmpty())
        info->addWidget(makeLabel(QStringLiteral("by %1").arg(m_product.brand), "muted"));
    info->addSpacing(6);

    auto *priceRow = new QHBoxLayout();
    priceRow->addWidget(makeLabel(m_product.price.toString(), "bigprice"));
    priceRow->addSpacing(12);
    priceRow->addWidget(makeBadge(stockText(m_product.stock, m_product.lowStockThreshold),
                                  stockBadgeKind(m_product.stock, m_product.lowStockThreshold)),
                        0, Qt::AlignVCenter);
    priceRow->addStretch();
    info->addLayout(priceRow);

    if (!m_product.description.isEmpty()) {
        auto *desc = makeLabel(m_product.description, "muted");
        desc->setWordWrap(true);
        info->addSpacing(6);
        info->addWidget(desc);
    }
    info->addSpacing(4);
    info->addWidget(makeLabel(QStringLiteral("SKU: %1").arg(m_product.sku), "small"));
    info->addStretch();

    if (ui.session.isCustomer()) {
        const PricingRules &rules = ui.app.pricing().rules();
        info->addWidget(makeLabel(QStringLiteral("Free delivery on orders over %1 · Cash on Delivery")
                                      .arg(rules.freeShippingThreshold.toString()),
                                  "small"));
        auto *buy = new QHBoxLayout();
        auto *qty = new QSpinBox(this);
        qty->setRange(1, qMax(1, qMin(m_product.stock, CartService::kMaxPerLine)));
        qty->setMinimumHeight(40);
        qty->setMinimumWidth(80);
        qty->setAccessibleName(QStringLiteral("Quantity"));
        qty->setEnabled(m_product.inStock());
        auto *add = makeButton(m_product.inStock() ? QStringLiteral("Add to cart") : QStringLiteral("Out of stock"), "primary");
        add->setMinimumHeight(40);
        add->setEnabled(m_product.inStock());
        buy->addWidget(qty);
        buy->addWidget(add, 1);
        info->addLayout(buy);

        connect(add, &QPushButton::clicked, this, [this, qty] {
            const Status st = m_ui.app.cart().add(m_ui.session, m_product.id, qty->value());
            if (!st) {
                showError(this, st.error());
                return;
            }
            emit m_ui.events->cartChanged();
            emit m_ui.events->toast(QStringLiteral("Added %1 × %2 to your cart").arg(qty->value()).arg(m_product.name));
            accept();
        });
    } else {
        QString adminInfo = QStringLiteral("Stock: %1 (low at %2)").arg(m_product.stock).arg(m_product.lowStockThreshold);
        if (m_product.cost)
            adminInfo += QStringLiteral("   ·   Cost: %1").arg(m_product.cost->toString());
        if (!m_product.isPurchasable())
            adminInfo += QStringLiteral("\nThis product is hidden from customers.");
        info->addWidget(makeLabel(adminInfo, "hint"));
    }

    auto *close = makeButton(QStringLiteral("Close"), "ghost");
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    info->addWidget(close, 0, Qt::AlignRight);
    layout->addLayout(info, 1);
}

// ====================================================================== Checkout
CheckoutDialog::CheckoutDialog(UiContext &ui, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_token(CheckoutService::newCheckoutToken())
{
    setWindowTitle(QStringLiteral("Checkout"));
    setMinimumSize(900, 580);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    root->addWidget(makeLabel(QStringLiteral("Checkout"), "h1"));

    auto *columns = new QHBoxLayout();
    columns->setSpacing(18);
    root->addLayout(columns, 1);

    // ---------------------------------------------------- delivery + payment
    auto *left = makeCard();
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(20, 18, 20, 18);
    lv->setSpacing(10);
    lv->addWidget(makeLabel(QStringLiteral("Delivery details"), "h2"));

    m_name = formField(QStringLiteral("Full name"));
    m_phone = formField(QStringLiteral("Phone, e.g. 0300 1234567"));
    m_address = formField(QStringLiteral("House, street, area"));
    m_city = formField(QStringLiteral("City"));
    auto *completer = new QCompleter(kCities, m_city);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_city->setCompleter(completer);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->addRow(makeFormLabel(QStringLiteral("Full name")), m_name);
    form->addRow(makeFormLabel(QStringLiteral("Phone")), m_phone);
    form->addRow(makeFormLabel(QStringLiteral("Address")), m_address);
    form->addRow(makeFormLabel(QStringLiteral("City")), m_city);
    lv->addLayout(form);

    m_saveAddress = new QCheckBox(QStringLiteral("Save as my default address"));
    m_saveAddress->setChecked(true);
    lv->addWidget(m_saveAddress);

    auto saved = ui.app.checkout().savedAddress(ui.session);
    if (saved && saved.value()) {
        m_name->setText(saved.value()->fullName);
        m_phone->setText(saved.value()->phone);
        m_address->setText(saved.value()->line1);
        m_city->setText(saved.value()->city);
    } else {
        m_name->setText(ui.session.fullName());
    }

    lv->addSpacing(6);
    lv->addWidget(makeDivider());
    lv->addSpacing(4);
    lv->addWidget(makeLabel(QStringLiteral("Payment method"), "h2"));
    auto *cod = new QRadioButton(QStringLiteral("Cash on Delivery"));
    cod->setChecked(true);
    lv->addWidget(cod);
    lv->addWidget(makeLabel(QStringLiteral("Pay in cash when your order arrives."), "small"));
    m_khata = new QRadioButton(QStringLiteral("Khata (pay later)"));
    m_khata->setAccessibleName(QStringLiteral("Pay with khata"));
    lv->addWidget(m_khata);
    auto *khataHint = makeLabel(QString(), "small");
    khataHint->setWordWrap(true);
    lv->addWidget(khataHint);
    auto *online = new QRadioButton(QStringLiteral("Card / JazzCash / Easypaisa  (coming soon)"));
    online->setEnabled(false);
    lv->addWidget(online);
    lv->addStretch();
    columns->addWidget(left, 3);

    // ---------------------------------------------------- summary
    auto *right = makeCard();
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(20, 18, 20, 18);
    rv->setSpacing(8);
    rv->addWidget(makeLabel(QStringLiteral("Order summary"), "h2"));

    auto items = ui.app.cart().items(ui.session);
    const QVector<CartItem> lines = items ? items.value() : QVector<CartItem>();
    const PriceBreakdown price = ui.app.pricing().price(lines);
    rv->addWidget(makeLabel(QStringLiteral("%1 item(s)").arg(price.itemCount), "small"));

    for (const CartItem &c : lines) {
        auto *row = new QHBoxLayout();
        auto *name = makeLabel(QStringLiteral("%1 × %2").arg(c.quantity).arg(c.name));
        name->setWordWrap(true);
        row->addWidget(name, 1);
        auto *total = makeLabel(c.lineTotal().toString());
        total->setAlignment(Qt::AlignRight | Qt::AlignTop);
        row->addWidget(total);
        rv->addLayout(row);
    }
    rv->addSpacing(4);
    rv->addWidget(makeDivider());
    rv->addLayout(summaryRow(QStringLiteral("Subtotal"), price.subtotal.toString()));
    rv->addLayout(summaryRow(QStringLiteral("Delivery"), price.shipping.isZero() ? QStringLiteral("Free") : price.shipping.toString()));
    if (!price.tax.isZero())
        rv->addLayout(summaryRow(QStringLiteral("Tax"), price.tax.toString()));
    rv->addWidget(makeDivider());
    rv->addLayout(summaryRow(QStringLiteral("Total"), price.total.toString(), true));

    // Khata is offered only when the shop has given this customer a credit limit.
    // The final check happens again in CheckoutService when the order is placed.
    auto khata = ui.app.khata().myAccount(ui.session);
    if (!khata || khata.value().creditLimit.isZero()) {
        m_khata->setEnabled(false);
        khataHint->setText(QStringLiteral("Khata is not open for your account. Ask the shop to give you a credit limit."));
    } else {
        const KhataAccount &k = khata.value();
        const bool fits = k.balance + price.total <= k.creditLimit;
        m_khata->setEnabled(fits);
        khataHint->setText(fits ? QStringLiteral("Available credit %1 of your %2 limit. Pay the shop later.")
                                      .arg(k.available().toString(), k.creditLimit.toString())
                                : QStringLiteral("Available credit is %1, less than this order. Pay some of your khata first.")
                                      .arg(k.available().toString()));
    }

    const Money threshold = ui.app.pricing().rules().freeShippingThreshold;
    if (!price.shipping.isZero() && price.subtotal < threshold)
        rv->addWidget(makeLabel(QStringLiteral("Add %1 more for free delivery.").arg((threshold - price.subtotal).toString()), "small"));
    rv->addStretch();

    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    rv->addWidget(m_error);

    m_placeButton = makeButton(QStringLiteral("Place order  ·  %1").arg(price.total.toString()), "primary");
    m_placeButton->setMinimumHeight(44);
    connect(m_placeButton, &QPushButton::clicked, this, &CheckoutDialog::placeOrder);
    rv->addWidget(m_placeButton);
    auto *back = makeButton(QStringLiteral("Back to cart"), "ghost");
    connect(back, &QPushButton::clicked, this, &QDialog::reject);
    rv->addWidget(back);
    columns->addWidget(right, 2);

    const QStringList problems = CartService::problems(lines);
    if (lines.isEmpty() || !problems.isEmpty()) {
        m_error->setText(lines.isEmpty() ? QStringLiteral("Your cart is empty.") : problems.join(QLatin1Char('\n')));
        m_placeButton->setEnabled(false);
    }
    if (!items)
        m_error->setText(items.error());
}

void CheckoutDialog::placeOrder()
{
    if (!m_placeButton->isEnabled())
        return;
    const QString idle = m_placeButton->text();
    m_placeButton->setEnabled(false);
    m_placeButton->setText(QStringLiteral("Placing your order..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    CheckoutRequest req;
    req.checkoutToken = m_token;
    req.shipping = {m_name->text(), m_phone->text(), m_address->text(), m_city->text()};
    req.saveAsDefaultAddress = m_saveAddress->isChecked();
    req.paymentMethod = m_khata->isChecked() ? payment::kKhata : payment::kCashOnDelivery;

    auto result = m_ui.app.checkout().placeOrder(m_ui.session, req);
    if (!result) {
        m_error->setText(result.error());
        m_placeButton->setText(idle);
        m_placeButton->setEnabled(true);
        emit m_ui.events->cartChanged(); // stock may have changed; let the cart page refresh
        return;
    }
    m_order = result.value();
    emit m_ui.events->cartChanged();
    emit m_ui.events->ordersChanged();
    accept();
}

// ====================================================================== Order details
OrderDetailsDialog::OrderDetailsDialog(UiContext &ui, qint64 orderId, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_orderId(orderId)
{
    setWindowTitle(QStringLiteral("Order details"));
    setMinimumSize(760, 600);
    m_body = new QVBoxLayout(this);
    m_body->setContentsMargins(24, 20, 24, 20);
    reload();
}

void OrderDetailsDialog::reload()
{
    auto result = m_ui.app.orders().details(m_ui.session, m_orderId);
    if (!result) {
        showError(parentWidget(), result.error());
        m_loaded = false;
        return;
    }
    m_order = result.value();
    m_loaded = true;
    auto history = m_ui.app.orders().history(m_ui.session, m_orderId);

    if (m_content) {
        m_body->removeWidget(m_content);
        m_content->deleteLater();
    }
    m_content = new QWidget(this);
    auto *v = new QVBoxLayout(m_content);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);
    m_body->addWidget(m_content);

    // Header
    auto *header = new QHBoxLayout();
    header->addWidget(makeLabel(QStringLiteral("Order %1").arg(m_order.orderNumber), "h1"));
    header->addSpacing(10);
    header->addWidget(makeBadge(orderstatus::displayName(m_order.status), Theme::badgeKind(m_order.status)), 0, Qt::AlignVCenter);
    header->addStretch();
    v->addLayout(header);

    QString paymentNote;
    if (m_order.paymentStatus == payment::kPaid)
        paymentNote = QStringLiteral("Paid");
    else if (m_order.paymentMethod == payment::kKhata)
        paymentNote = m_order.status == OrderStatus::Cancelled ? QStringLiteral("Taken off the khata")
                                                               : QStringLiteral("Added to the khata");
    else
        paymentNote = QStringLiteral("Payment due on delivery");
    QString meta = QStringLiteral("Placed %1  ·  %2  ·  %3")
                       .arg(formatDateTime(m_order.createdAt), payment::methodDisplay(m_order.paymentMethod), paymentNote);
    if (m_ui.session.isAdmin())
        meta += QStringLiteral("\nCustomer: %1 (%2)").arg(m_order.customerName, m_order.customerEmail);
    v->addWidget(makeLabel(meta, "muted"));

    // Items
    auto *table = new QTableWidget(m_order.items.size(), 5, m_content);
    table->setHorizontalHeaderLabels({QStringLiteral("Product"), QStringLiteral("SKU"), QStringLiteral("Unit price"),
                                      QStringLiteral("Qty"), QStringLiteral("Total")});
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 5; ++c)
        table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    for (int r = 0; r < m_order.items.size(); ++r) {
        const OrderItem &it = m_order.items[r];
        const QStringList cells = {it.productName, it.sku, it.unitPrice.toString(), QString::number(it.quantity),
                                   it.lineTotal.toString()};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            if (c >= 2)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, c, item);
        }
    }
    table->verticalHeader()->setDefaultSectionSize(34);
    table->setFixedHeight(qMin(240, 42 + 34 * int(m_order.items.size())));
    v->addWidget(table);

    // Delivery + totals
    auto *row = new QHBoxLayout();
    row->setSpacing(14);
    auto *ship = makeCard();
    auto *sv = new QVBoxLayout(ship);
    sv->setContentsMargins(16, 14, 16, 14);
    sv->addWidget(makeLabel(QStringLiteral("Deliver to"), "h3"));
    auto *addr = makeLabel(QStringLiteral("%1\n%2\n%3, %4").arg(m_order.shipName, m_order.shipPhone, m_order.shipLine1, m_order.shipCity));
    addr->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sv->addWidget(addr);
    sv->addStretch();
    row->addWidget(ship, 1);

    auto *totals = makeCard();
    auto *tv = new QVBoxLayout(totals);
    tv->setContentsMargins(16, 14, 16, 14);
    tv->addLayout(summaryRow(QStringLiteral("Subtotal"), m_order.subtotal.toString()));
    tv->addLayout(summaryRow(QStringLiteral("Delivery"), m_order.shipping.isZero() ? QStringLiteral("Free") : m_order.shipping.toString()));
    if (!m_order.tax.isZero())
        tv->addLayout(summaryRow(QStringLiteral("Tax"), m_order.tax.toString()));
    tv->addWidget(makeDivider());
    tv->addLayout(summaryRow(QStringLiteral("Total"), m_order.total.toString(), true));
    row->addWidget(totals, 1);
    v->addLayout(row);

    // Timeline
    v->addWidget(makeLabel(QStringLiteral("Status history"), "h3"));
    if (history) {
        for (const OrderStatusChange &c : history.value()) {
            auto *line = new QHBoxLayout();
            line->addWidget(makeBadge(orderstatus::displayName(c.to), Theme::badgeKind(c.to)), 0, Qt::AlignTop);
            QString text = formatDateTime(c.changedAt);
            if (!c.changedBy.isEmpty())
                text += QStringLiteral("  ·  by %1").arg(c.changedBy);
            if (!c.note.isEmpty())
                text += QStringLiteral("\n") + c.note;
            auto *l = makeLabel(text, "muted");
            l->setWordWrap(true);
            line->addSpacing(8);
            line->addWidget(l, 1);
            v->addLayout(line);
        }
    }
    v->addStretch();

    // Actions
    auto *actions = new QHBoxLayout();
    const bool canCancel = m_ui.session.isAdmin() ? orderstatus::adminCanCancel(m_order.status)
                                                  : orderstatus::customerCanCancel(m_order.status);
    if (canCancel) {
        auto *cancel = makeButton(QStringLiteral("Cancel order"), "danger");
        connect(cancel, &QPushButton::clicked, this, &OrderDetailsDialog::cancelOrder);
        actions->addWidget(cancel);
    }
    actions->addStretch();
    if (m_ui.session.isAdmin()) {
        if (const auto next = orderstatus::nextForward(m_order.status)) {
            auto *advanceBtn = makeButton(QStringLiteral("Mark as %1").arg(orderstatus::displayName(*next)), "primary");
            connect(advanceBtn, &QPushButton::clicked, this, &OrderDetailsDialog::advance);
            actions->addWidget(advanceBtn);
        }
    }
    auto *close = makeButton(QStringLiteral("Close"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    actions->addWidget(close);
    v->addLayout(actions);
}

void OrderDetailsDialog::advance()
{
    const auto next = orderstatus::nextForward(m_order.status);
    if (!next)
        return;
    const Status st = m_ui.app.orders().advance(m_ui.session, m_orderId, *next);
    if (!st) {
        showError(this, st.error());
        return;
    }
    emit m_ui.events->ordersChanged();
    emit m_ui.events->toast(QStringLiteral("Order %1 marked as %2").arg(m_order.orderNumber, orderstatus::displayName(*next)));
    reload();
}

void OrderDetailsDialog::cancelOrder()
{
    if (!confirm(this, QStringLiteral("Cancel order"),
                 QStringLiteral("Cancel order %1? The items will be returned to stock.").arg(m_order.orderNumber),
                 QStringLiteral("Cancel order")))
        return;
    const Status st = m_ui.app.orders().cancel(m_ui.session, m_orderId, QString());
    if (!st) {
        showError(this, st.error());
        return;
    }
    emit m_ui.events->ordersChanged();
    emit m_ui.events->catalogChanged();
    emit m_ui.events->toast(QStringLiteral("Order %1 cancelled").arg(m_order.orderNumber));
    reload();
}

// ====================================================================== Change password
ChangePasswordDialog::ChangePasswordDialog(UiContext &ui, QWidget *parent)
    : QDialog(parent), m_ui(ui)
{
    setWindowTitle(QStringLiteral("Change password"));
    setMinimumWidth(420);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    v->addWidget(makeLabel(QStringLiteral("Change password"), "h2"));

    auto *current = formField(QStringLiteral("Current password"));
    auto *next = formField(QStringLiteral("New password"));
    auto *again = formField(QStringLiteral("Confirm new password"));
    for (QLineEdit *e : {current, next, again}) {
        e->setEchoMode(QLineEdit::Password);
        v->addWidget(e);
    }
    v->addWidget(makeLabel(QStringLiteral("At least 8 characters, with a letter and a number."), "small"));
    auto *error = makeLabel(QString(), "error");
    error->setWordWrap(true);
    v->addWidget(error);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = makeButton(QStringLiteral("Cancel"), "ghost");
    auto *save = makeButton(QStringLiteral("Update password"), "primary");
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    v->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this, current, next, again, error, save] {
        if (next->text() != again->text()) {
            error->setText(QStringLiteral("The new passwords do not match."));
            return;
        }
        save->setEnabled(false);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const Status st = m_ui.app.auth().changePassword(m_ui.session, current->text(), next->text());
        QApplication::restoreOverrideCursor();
        save->setEnabled(true);
        if (!st) {
            error->setText(st.error());
            return;
        }
        emit m_ui.events->toast(QStringLiteral("Your password has been updated"));
        accept();
    });
}

} // namespace shophub::ui
