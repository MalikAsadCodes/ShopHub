#pragma once

#include "common/Money.h"
#include "domain/OrderStatus.h"

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
    Money unitPrice;
    int quantity = 0;
    int availableStock = 0;
    bool productActive = true;

    Money lineTotal() const { return unitPrice * quantity; }
    bool isAvailable() const { return productActive && quantity <= availableStock; }
};

struct PriceBreakdown
{
    Money subtotal;
    Money shipping;
    Money tax;
    Money total;
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
    QString paymentMethod;  // "cod"
    QString paymentStatus;  // "unpaid" | "paid"
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
    QVector<TopProduct> topProducts;
    QVector<DailySales> last14Days;
    QVector<Order> recentOrders;
};

} // namespace shophub
