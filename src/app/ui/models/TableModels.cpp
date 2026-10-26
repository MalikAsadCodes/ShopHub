#include "ui/models/TableModels.h"

#include "ui/Theme.h"
#include "ui/UiCommon.h"

#include <QFont>
#include <QLocale>

namespace shophub::ui {

// ============================================================== Products
ProductTableModel::ProductTableModel(QVector<Column> columns, QObject *parent)
    : QAbstractTableModel(parent), m_columns(std::move(columns))
{
}

void ProductTableModel::setProducts(QVector<Product> products)
{
    beginResetModel();
    m_products = std::move(products);
    endResetModel();
}

const Product *ProductTableModel::productAt(int row) const
{
    return row >= 0 && row < m_products.size() ? &m_products[row] : nullptr;
}

int ProductTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_products.size();
}

int ProductTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columns.size();
}

QVariant ProductTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= m_columns.size())
        return QVariant();
    switch (m_columns[section]) {
    case Sku: return QStringLiteral("SKU");
    case Name: return QStringLiteral("Product");
    case Category: return QStringLiteral("Category");
    case Brand: return QStringLiteral("Brand");
    case Price: return QStringLiteral("Price");
    case Cost: return QStringLiteral("Cost");
    case Stock: return QStringLiteral("Stock");
    case LowAt: return QStringLiteral("Low at");
    case StockState: return QStringLiteral("Stock status");
    case Status: return QStringLiteral("Status");
    case Wholesale: return QStringLiteral("Qty prices");
    case Qist: return QStringLiteral("Qist");
    }
    return QVariant();
}

QVariant ProductTableModel::data(const QModelIndex &index, int role) const
{
    const Product *p = productAt(index.row());
    if (!p || index.column() >= m_columns.size())
        return QVariant();
    const Column col = m_columns[index.column()];
    const ThemePalette &pal = Theme::palette();

    if (role == kIdRole)
        return p->id;

    if (role == Qt::DisplayRole || role == kSortRole) {
        const bool sort = role == kSortRole;
        switch (col) {
        case Sku: return p->sku;
        case Name: return p->name;
        case Category: return p->categoryName;
        case Brand: return p->brand;
        case Price: return sort ? QVariant(p->price.paisa()) : QVariant(p->price.toString());
        case Cost:
            if (sort)
                return p->cost ? p->cost->paisa() : -1;
            return p->cost ? p->cost->toString() : QStringLiteral("-");
        case Stock: return p->stock;
        case LowAt: return p->lowStockThreshold;
        case StockState: return sort ? QVariant(p->stock - p->lowStockThreshold) : QVariant(stockText(p->stock, p->lowStockThreshold));
        case Status:
            if (!p->active)
                return QStringLiteral("Inactive");
            if (!p->categoryActive)
                return QStringLiteral("Category off");
            return QStringLiteral("Active");
        case Wholesale:
            if (sort)
                return p->wholesaleFrom > 0 ? p->wholesaleFrom : 1000;
            return p->wholesaleFrom > 0 ? QStringLiteral("from %1").arg(p->wholesaleFrom) : QStringLiteral("-");
        case Qist:
            if (sort)
                return p->qistAllowed ? 0 : 1;
            return p->qistAllowed ? QStringLiteral("Yes") : QStringLiteral("-");
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (col == Price || col == Cost || col == Stock || col == LowAt)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (col == StockState || col == Stock) {
            if (p->stock <= 0)
                return pal.danger;
            if (p->isLowStock())
                return pal.warning;
            return col == StockState ? QVariant(pal.success) : QVariant();
        }
        if (col == Status)
            return p->isPurchasable() ? pal.success : pal.muted;
        if (col == Qist && p->qistAllowed)
            return pal.info;
        if (!p->active)
            return pal.muted;
    }

    if (role == Qt::FontRole && (col == Name || col == StockState)) {
        QFont f;
        f.setBold(true);
        return f;
    }
    return QVariant();
}

// ============================================================== Orders
OrderTableModel::OrderTableModel(Mode mode, QObject *parent)
    : QAbstractTableModel(parent)
{
    if (mode == Mode::Customer)
        m_columns = {Number, Placed, Items, Total, State};
    else
        m_columns = {Number, Placed, Customer, City, Items, Total, Payment, State};
}

void OrderTableModel::setOrders(QVector<Order> orders)
{
    beginResetModel();
    m_orders = std::move(orders);
    endResetModel();
}

const Order *OrderTableModel::orderAt(int row) const
{
    return row >= 0 && row < m_orders.size() ? &m_orders[row] : nullptr;
}

int OrderTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_orders.size();
}

int OrderTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columns.size();
}

QVariant OrderTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= m_columns.size())
        return QVariant();
    switch (m_columns[section]) {
    case Number: return QStringLiteral("Order #");
    case Placed: return QStringLiteral("Placed");
    case Customer: return QStringLiteral("Customer");
    case City: return QStringLiteral("City");
    case Items: return QStringLiteral("Items");
    case Total: return QStringLiteral("Total");
    case Payment: return QStringLiteral("Payment");
    case State: return QStringLiteral("Status");
    }
    return QVariant();
}

QVariant OrderTableModel::data(const QModelIndex &index, int role) const
{
    const Order *o = orderAt(index.row());
    if (!o || index.column() >= m_columns.size())
        return QVariant();
    const Column col = m_columns[index.column()];

    if (role == kIdRole)
        return o->id;

    if (role == Qt::DisplayRole || role == kSortRole) {
        const bool sort = role == kSortRole;
        switch (col) {
        case Number: return o->orderNumber;
        case Placed: return sort ? QVariant(o->createdAt) : QVariant(formatDateTime(o->createdAt));
        case Customer: return o->customerName;
        case City: return o->shipCity;
        case Items: return o->itemCount;
        case Total: return sort ? QVariant(o->total.paisa()) : QVariant(o->total.toString());
        case Payment:
            if (o->paymentMethod == payment::kKhata)
                return o->status == OrderStatus::Cancelled ? QStringLiteral("Khata - reversed") : QStringLiteral("Khata");
            if (o->paymentMethod == payment::kQist) {
                if (o->status == OrderStatus::Cancelled)
                    return QStringLiteral("Qist - cancelled");
                return o->paymentStatus == payment::kPaid ? QStringLiteral("Qist - paid") : QStringLiteral("Qist");
            }
            return QStringLiteral("COD - %1").arg(payment::statusDisplay(o->paymentStatus));
        case State: return orderstatus::displayName(o->status);
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (col == Total || col == Items)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole && col == State)
        return Theme::statusColor(o->status);
    if (role == Qt::ForegroundRole && col == Payment && o->paymentStatus == payment::kPaid)
        return Theme::palette().success;
    if (role == Qt::ForegroundRole && col == Payment && o->paymentMethod == payment::kKhata)
        return o->status == OrderStatus::Cancelled ? Theme::palette().muted : Theme::palette().accent;
    if (role == Qt::ForegroundRole && col == Payment && o->paymentMethod == payment::kQist)
        return o->status == OrderStatus::Cancelled ? Theme::palette().muted : Theme::palette().info;
    if (role == Qt::FontRole && (col == State || col == Number)) {
        QFont f;
        f.setBold(true);
        return f;
    }
    return QVariant();
}

// ============================================================== Movements
MovementTableModel::MovementTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void MovementTableModel::setMovements(QVector<InventoryMovement> movements)
{
    beginResetModel();
    m_rows = std::move(movements);
    endResetModel();
}

int MovementTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int MovementTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 7;
}

QVariant MovementTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    static const char *names[] = {"When", "Product", "Change", "Stock after", "Reason", "Note / reference", "By"};
    return section >= 0 && section < 7 ? QString::fromLatin1(names[section]) : QVariant();
}

QVariant MovementTableModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    const InventoryMovement &m = m_rows[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return formatDateTime(m.createdAt);
        case 1: return m.productName;
        case 2: return m.change > 0 ? QStringLiteral("+%1").arg(m.change) : QString::number(m.change);
        case 3: return m.stockAfter;
        case 4: return stockReasonDisplay(m.reason);
        case 5: return m.note;
        case 6: return m.userName;
        }
    }
    if (role == Qt::ForegroundRole && index.column() == 2)
        return m.change > 0 ? Theme::palette().success : Theme::palette().danger;
    if (role == Qt::TextAlignmentRole && (index.column() == 2 || index.column() == 3))
        return int(Qt::AlignRight | Qt::AlignVCenter);
    return QVariant();
}

// ============================================================== Khata accounts
namespace {
enum KhataAccountColumn { KCustomer, KEmail, KLimit, KBalance, KAvailable, KQist, KLast, KAccountColumns };
}

KhataAccountTableModel::KhataAccountTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void KhataAccountTableModel::setAccounts(QVector<KhataAccount> accounts)
{
    beginResetModel();
    m_rows = std::move(accounts);
    endResetModel();
}

const KhataAccount *KhataAccountTableModel::accountAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows[row] : nullptr;
}

int KhataAccountTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int KhataAccountTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : KAccountColumns;
}

QVariant KhataAccountTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    static const char *names[] = {"Customer", "Email", "Credit limit", "Owes", "Available", "Qist", "Last activity"};
    return section >= 0 && section < KAccountColumns ? QString::fromLatin1(names[section]) : QVariant();
}

QVariant KhataAccountTableModel::data(const QModelIndex &index, int role) const
{
    const KhataAccount *a = accountAt(index.row());
    if (!a)
        return QVariant();
    const int col = index.column();
    if (role == kIdRole)
        return a->userId;
    if (role == Qt::DisplayRole || role == kSortRole) {
        const bool sort = role == kSortRole;
        auto money = [sort](Money m) { return sort ? QVariant(m.paisa()) : QVariant(m.toString()); };
        switch (col) {
        case KCustomer: return a->customerName;
        case KEmail: return a->email;
        case KLimit:
            if (!sort && a->creditLimit.isZero())
                return QStringLiteral("Not open");
            return money(a->creditLimit);
        case KBalance:
            if (!sort && a->balance.isNegative())
                return QStringLiteral("Advance %1").arg((Money() - a->balance).toString());
            return money(a->balance);
        case KAvailable: return money(a->available());
        case KQist:
            // What is left on qist, or whether the customer may buy on qist.
            if (sort)
                return a->qistBalance > Money() ? QVariant(a->qistBalance.paisa()) : QVariant(a->qistApproved ? 0 : -1);
            if (a->qistBalance > Money())
                return a->qistBalance.toString();
            return a->qistApproved ? QStringLiteral("Allowed") : QStringLiteral("-");
        case KLast:
            if (sort)
                return QVariant(a->lastActivity);
            return a->lastActivity.isValid() ? formatDate(a->lastActivity) : QStringLiteral("-");
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (col == KLimit || col == KBalance || col == KAvailable || col == KQist)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole && col == KBalance) {
        if (a->overLimit())
            return Theme::palette().danger;
        if (a->balance > Money())
            return Theme::palette().warning;
        if (a->balance.isNegative())
            return Theme::palette().success;
    }
    if (role == Qt::ForegroundRole && col == KLimit && a->creditLimit.isZero())
        return Theme::palette().muted;
    if (role == Qt::ForegroundRole && col == KQist)
        return a->qistBalance > Money() || a->qistApproved ? Theme::palette().info : Theme::palette().muted;
    if (role == Qt::FontRole && col == KBalance && a->balance > Money()) {
        QFont f;
        f.setBold(true);
        return f;
    }
    return QVariant();
}

// ============================================================== Khata ledger
KhataEntryTableModel::KhataEntryTableModel(bool showAuthor, QObject *parent)
    : QAbstractTableModel(parent), m_showAuthor(showAuthor)
{
}

void KhataEntryTableModel::setEntries(QVector<KhataEntry> entries)
{
    beginResetModel();
    m_rows = std::move(entries);
    endResetModel();
}

int KhataEntryTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int KhataEntryTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : (m_showAuthor ? 7 : 6);
}

QVariant KhataEntryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    static const char *names[] = {"When", "Entry", "Order", "Note", "Amount", "Balance", "By"};
    return section >= 0 && section < columnCount() ? QString::fromLatin1(names[section]) : QVariant();
}

QVariant KhataEntryTableModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    const KhataEntry &e = m_rows[index.row()];
    const int col = index.column();
    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: return formatDateTime(e.createdAt);
        case 1:
            if (e.qistId > 0) {
                switch (e.kind) {
                case KhataKind::Charge: return QStringLiteral("Bought on qist");
                case KhataKind::Payment: return QStringLiteral("Qist installment");
                case KhataKind::Reversal: return QStringLiteral("Qist cancelled");
                case KhataKind::Adjustment: return QStringLiteral("Qist refund");
                }
            }
            return khataKindDisplay(e.kind);
        case 2: return e.orderNumber.isEmpty() ? QStringLiteral("-") : e.orderNumber;
        case 3:
            if (e.qistId == 0 && (e.kind == KhataKind::Charge || e.kind == KhataKind::Reversal))
                return QString(); // the note is the order number, already shown
            return e.kind == KhataKind::Reversal ? QString() : e.note;
        case 4: return e.amount.isNegative() ? e.amount.toString() : QStringLiteral("+") + e.amount.toString();
        case 5: return e.balanceAfter.toString();
        case 6: return e.createdBy;
        }
    }
    // Red = the customer owes more, green = they owe less.
    if (role == Qt::ForegroundRole && col == 4)
        return e.amount.isNegative() ? Theme::palette().success : Theme::palette().danger;
    if (role == Qt::ForegroundRole && col == 1 && e.qistId > 0)
        return Theme::palette().info;
    if (role == Qt::TextAlignmentRole && (col == 4 || col == 5))
        return int(Qt::AlignRight | Qt::AlignVCenter);
    return QVariant();
}

// ============================================================== Qist accounts
QistAccountTableModel::QistAccountTableModel(bool showCustomer, QObject *parent)
    : QAbstractTableModel(parent)
{
    if (showCustomer)
        m_columns = {Customer, Order, Plan, Price, Paid, Left, NextDue, State};
    else
        m_columns = {Order, Plan, Price, Paid, Left, NextDue, State};
}

void QistAccountTableModel::setAccounts(QVector<QistAccount> accounts)
{
    beginResetModel();
    m_rows = std::move(accounts);
    endResetModel();
}

const QistAccount *QistAccountTableModel::accountAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows[row] : nullptr;
}

int QistAccountTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int QistAccountTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columns.size();
}

QVariant QistAccountTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= m_columns.size())
        return QVariant();
    switch (m_columns[section]) {
    case Customer: return QStringLiteral("Customer");
    case Order: return QStringLiteral("Order #");
    case Plan: return QStringLiteral("Plan");
    case Price: return QStringLiteral("Qist price");
    case Paid: return QStringLiteral("Paid");
    case Left: return QStringLiteral("Left");
    case NextDue: return QStringLiteral("Next due");
    case State: return QStringLiteral("Status");
    }
    return QVariant();
}

QVariant QistAccountTableModel::data(const QModelIndex &index, int role) const
{
    const QistAccount *a = accountAt(index.row());
    if (!a || index.column() >= m_columns.size())
        return QVariant();
    const Column col = m_columns[index.column()];
    const ThemePalette &pal = Theme::palette();

    if (role == kIdRole)
        return a->id;

    if (role == Qt::DisplayRole || role == kSortRole) {
        const bool sort = role == kSortRole;
        auto money = [sort](Money m) { return sort ? QVariant(m.paisa()) : QVariant(m.toString()); };
        const std::optional<InstallmentStatus> next = a->nextDue();
        switch (col) {
        case Customer: return a->customerName;
        case Order: return a->orderNumber;
        case Plan: return a->planName;
        case Price: return money(a->qistPrice);
        case Paid: return money(a->paid);
        case Left: return money(a->left());
        case NextDue:
            if (sort)
                return next ? QVariant(next->installment.dueDate) : QVariant(QDate(9999, 12, 31));
            if (!next)
                return QStringLiteral("-");
            return QStringLiteral("%1  ·  %2")
                .arg(QLocale(QLocale::English).toString(next->installment.dueDate, QStringLiteral("d MMM yyyy")),
                     next->remaining().toString());
        case State: return a->statusText();
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (col == Price || col == Paid || col == Left)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole && col == State) {
        if (a->state == QistState::Completed)
            return pal.success;
        if (a->state == QistState::Cancelled)
            return pal.muted;
        return a->isOverdue() ? pal.danger : pal.info;
    }
    if (role == Qt::ForegroundRole && col == NextDue && a->isOverdue())
        return pal.danger;
    if (role == Qt::FontRole && (col == State || col == Order)) {
        QFont f;
        f.setBold(true);
        return f;
    }
    return QVariant();
}

// ============================================================== Qist schedule
QistScheduleTableModel::QistScheduleTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void QistScheduleTableModel::setAccount(const QistAccount &account)
{
    beginResetModel();
    m_rows = account.installments;
    m_months = account.terms.months;
    m_state = account.state;
    endResetModel();
}

void QistScheduleTableModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int QistScheduleTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int QistScheduleTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant QistScheduleTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    static const char *names[] = {"Installment", "Due date", "Amount", "Paid", "Status"};
    return section >= 0 && section < 5 ? QString::fromLatin1(names[section]) : QVariant();
}

QVariant QistScheduleTableModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    const InstallmentStatus &s = m_rows[index.row()];
    const int col = index.column();
    // A cancelled qist has nothing left to pay; show the lines without a state.
    const bool cancelled = m_state == QistState::Cancelled;
    if (role == Qt::DisplayRole) {
        switch (col) {
        case 0: return qist::installmentName(s.installment.number, m_months);
        case 1: return QLocale(QLocale::English).toString(s.installment.dueDate, QStringLiteral("d MMM yyyy"));
        case 2: return s.installment.amount.toString();
        case 3: return s.paid.isZero() ? QStringLiteral("-") : s.paid.toString();
        case 4:
            if (cancelled)
                return s.state == InstallmentState::Paid ? QStringLiteral("Paid (returned)") : QStringLiteral("Cancelled");
            return qist::stateText(s.state);
        }
    }
    if (role == Qt::TextAlignmentRole && (col == 2 || col == 3))
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole && col == 4 && !cancelled) {
        switch (s.state) {
        case InstallmentState::Paid: return Theme::palette().success;
        case InstallmentState::Overdue: return Theme::palette().danger;
        case InstallmentState::DueToday: return Theme::palette().warning;
        case InstallmentState::Upcoming: return Theme::palette().muted;
        }
    }
    if (role == Qt::FontRole && col == 4 && s.state != InstallmentState::Upcoming) {
        QFont f;
        f.setBold(true);
        return f;
    }
    return QVariant();
}

} // namespace shophub::ui
