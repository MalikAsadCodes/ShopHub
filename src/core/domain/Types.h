#pragma once

#include "common/Money.h"
#include "domain/OrderStatus.h"
#include "domain/PriceTier.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>

namespace shophub {

// ---------------------------------------------------------------- Users
enum class Role { Customer, Admin };

QString roleToDb(Role role);
std::optional<Role> roleFromDb(const QString &s);

struct User
{
    qint64 id = 0;
    QString email;
    QString fullName;
    Role role = Role::Customer;
    bool active = true;
    QDateTime createdAt;
};

// User plus the private fields only the auth layer needs.
struct UserRecord
{
    User user;
    QString passwordHash;
    int failedLogins = 0;
    QDateTime lockedUntil; // invalid when not locked
};

struct Address
{
    qint64 id = 0;
    qint64 userId = 0;
    QString fullName;
    QString phone;
    QString line1;
    QString city;
};

// ---------------------------------------------------------------- Catalog
struct Category
{
    qint64 id = 0;
    QString name;
    bool active = true;
    int productCount = 0;
};

struct Product
{
    qint64 id = 0;
    QString sku;
    QString name;
    QString description;
    qint64 categoryId = 0;
    QString categoryName;
    QString brand;
    Money price;
    std::optional<Money> cost; // admin only; stripped for customers
    int stock = 0;
    int lowStockThreshold = 5;
    bool active = true;          // the product's own on/off switch
    bool categoryActive = true;  // false when its category is switched off
    QString imageFile; // file name inside the app's image store, or empty
    int unitsSold = 0;
    int wholesaleFrom = 0; // smallest quantity with a wholesale price, 0 = no quantity prices
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isPurchasable() const { return active && categoryActive; }
    bool inStock() const { return stock > 0; }
    bool isLowStock() const { return stock <= lowStockThreshold; }
};

// What an administrator types into the product form.
struct ProductInput
{
    QString sku;
    QString name;
    QString description;
    qint64 categoryId = 0;
    QString brand;
    Money price;
    std::optional<Money> cost;
    int initialStock = 0; // used on create only; later changes go through inventory
    int lowStockThreshold = 5;
    QString imageFile;
};

enum class ProductSort { Newest, PriceLowToHigh, PriceHighToLow, NameAZ, Popularity };

struct ProductFilter
{
    QString text;            // matches name, SKU, brand, description
    qint64 categoryId = 0;   // 0 = all
    QString brand;           // empty = all
    std::optional<Money> minPrice;
    std::optional<Money> maxPrice;
    bool inStockOnly = false;
    bool includeInactive = false; // honoured for admins only
    ProductSort sort = ProductSort::Newest;
    int limit = 24;
    int offset = 0;
};

struct ProductPage
{
    QVector<Product> items;
    int total = 0; // total matches, ignoring limit/offset
};

// ---------------------------------------------------------------- Cart & pricing
struct CartItem
{
    qint64 productId = 0;
    QString sku;
    QString name;
    QString imageFile;
    QString categoryName;
    Money basePrice;   // the product's normal price
    Money unitPrice;   // the price charged per unit: basePrice, or lower when a wholesale tier applies
    int quantity = 0;
    int availableStock = 0;
    bool productActive = true;

    // Wholesale prices, filled in by the services (PricingService::applyTiers).
    int discountBp = 0;                  // the tier applied to this line, 0 = normal price
    std::optional<PriceTier> nextTier;   // the next tier up, for "add 3 more for Rs X each"
    Money nextTierUnitPrice;             // unit price at nextTier (zero when there is none)

    Money lineTotal() const { return unitPrice * quantity; }
    Money lineSavings() const { return basePrice > unitPrice ? (basePrice - unitPrice) * quantity : Money(); }
    int unitsToNextTier() const { return nextTier ? nextTier->minQuantity - quantity : 0; }
    bool isAvailable() const { return productActive && quantity <= availableStock; }
};

struct PriceBreakdown
{
    Money subtotal;    // after wholesale discounts
    Money shipping;
    Money tax;
    Money total;
    Money savings;     // what wholesale prices saved, already taken off the subtotal
    int itemCount = 0; // sum of quantities
};

// ---------------------------------------------------------------- Orders
struct OrderItem
{
    qint64 id = 0;
    std::optional<qint64> productId; // null if the product was later removed
    QString sku;
    QString productName;
    Money unitPrice;
    int quantity = 0;
    Money lineTotal;
};

struct Order
{
    qint64 id = 0;
    QString orderNumber;
    qint64 userId = 0;
    QString customerName;
    QString customerEmail;
    OrderStatus status = OrderStatus::Pending;
    Money subtotal;
    Money shipping;
    Money tax;
    Money total;
    QString paymentMethod;  // "cod" | "khata"
    QString paymentStatus;  // "unpaid" | "paid" | "on_account"
    QString shipName;
    QString shipPhone;
    QString shipLine1;
    QString shipCity;
    QString checkoutToken;
    QDateTime createdAt;
    QDateTime updatedAt;
    int itemCount = 0;
    QVector<OrderItem> items;
};

struct OrderStatusChange
{
    std::optional<OrderStatus> from;
    OrderStatus to = OrderStatus::Pending;
    QString changedBy;
    QString note;
    QDateTime changedAt;
};

// ---------------------------------------------------------------- Inventory
enum class StockReason { Initial, Restock, Adjustment, Damaged, Sale, CancelRestock };

QString stockReasonToDb(StockReason r);
std::optional<StockReason> stockReasonFromDb(const QString &s);
QString stockReasonDisplay(StockReason r);

struct InventoryMovement
{
    qint64 id = 0;
    qint64 productId = 0;
    QString productName;
    int change = 0;
    int stockAfter = 0;
    StockReason reason = StockReason::Adjustment;
    QString note;
    QString userName;
    QDateTime createdAt;
};

// ---------------------------------------------------------------- Khata (credit ledger)
namespace payment {
inline const QString kCashOnDelivery = QStringLiteral("cod");
inline const QString kKhata = QStringLiteral("khata");
inline const QString kUnpaid = QStringLiteral("unpaid");
inline const QString kPaid = QStringLiteral("paid");
inline const QString kOnAccount = QStringLiteral("on_account");

QString methodDisplay(const QString &method);   // "Cash on Delivery", "Khata"
QString statusDisplay(const QString &status);   // "Unpaid", "Paid", "On khata"
} // namespace payment

// charge: an order bought on credit (raises the balance)
// payment: money received from the customer (lowers it)
// reversal: a khata order was cancelled (lowers it)
// adjustment: a correction or write-off by the shop, with a note (either way)
enum class KhataKind { Charge, Payment, Reversal, Adjustment };

QString khataKindToDb(KhataKind k);
std::optional<KhataKind> khataKindFromDb(const QString &s);
QString khataKindDisplay(KhataKind k);

struct KhataEntry
{
    qint64 id = 0;
    qint64 userId = 0;
    KhataKind kind = KhataKind::Charge;
    Money amount;        // > 0 raises what the customer owes, < 0 lowers it
    Money balanceAfter;
    qint64 orderId = 0;  // 0 when not linked to an order
    QString orderNumber;
    QString note;
    QString createdBy;
    QDateTime createdAt;
};

struct KhataAccount
{
    qint64 userId = 0;
    QString customerName;
    QString email;
    Money creditLimit;   // 0 = no credit allowed
    Money balance;       // what the customer owes right now (< 0 = advance held by the shop)
    QDateTime lastActivity; // invalid when there are no entries

    Money available() const { return balance >= creditLimit ? Money() : creditLimit - balance; }
    bool overLimit() const { return balance > creditLimit; }
};

// ---------------------------------------------------------------- Dashboard
struct TopProduct
{
    QString name;
    int units = 0;
    Money revenue;
};

struct DailySales
{
    QDate date;
    Money revenue;
    int orders = 0;
};

struct DashboardStats
{
    Money grossSales;        // non-cancelled orders, incl. shipping and tax
    int orderCount = 0;      // non-cancelled orders
    Money averageOrderValue;
    int pendingOrders = 0;   // pending + confirmed + processing
    int lowStockCount = 0;
    int customerCount = 0;
    Money khataOutstanding;  // total owed by all customers on khata
    int khataDebtors = 0;    // customers whose khata balance is above zero
    QVector<TopProduct> topProducts;
    QVector<DailySales> last14Days;
    QVector<Order> recentOrders;
};

} // namespace shophub
