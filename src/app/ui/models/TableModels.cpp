#include "ui/models/TableModels.h"

#include "ui/Theme.h"
#include "ui/UiCommon.h"

#include <QFont>

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
            return QStringLiteral("COD - %1").arg(o->paymentStatus == QLatin1String("paid") ? QStringLiteral("Paid")
                                                                                          : QStringLiteral("Unpaid"));
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
    if (role == Qt::ForegroundRole && col == Payment && o->paymentStatus == QLatin1String("paid"))
        return Theme::palette().success;
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

} // namespace shophub::ui
