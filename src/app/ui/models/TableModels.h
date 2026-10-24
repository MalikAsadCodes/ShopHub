#pragma once

#include "domain/Types.h"

#include <QAbstractTableModel>

namespace shophub::ui {

// Qt::UserRole returns a raw value for correct numeric sorting through
// QSortFilterProxyModel (set its sortRole to Qt::UserRole).
constexpr int kSortRole = Qt::UserRole;
constexpr int kIdRole = Qt::UserRole + 1;

class ProductTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Sku, Name, Category, Brand, Price, Cost, Stock, LowAt, StockState, Status, Wholesale };

    explicit ProductTableModel(QVector<Column> columns, QObject *parent = nullptr);

    void setProducts(QVector<Product> products);
    const Product *productAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<Column> m_columns;
    QVector<Product> m_products;
};

class OrderTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum class Mode { Customer, Admin };

    explicit OrderTableModel(Mode mode, QObject *parent = nullptr);

    void setOrders(QVector<Order> orders);
    const Order *orderAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    enum Column { Number, Placed, Customer, City, Items, Total, Payment, State };
    QVector<Column> m_columns;
    QVector<Order> m_orders;
};

class MovementTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit MovementTableModel(QObject *parent = nullptr);

    void setMovements(QVector<InventoryMovement> movements);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<InventoryMovement> m_rows;
};

// Customers with their khata limit and balance (admin).
class KhataAccountTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit KhataAccountTableModel(QObject *parent = nullptr);

    void setAccounts(QVector<KhataAccount> accounts);
    const KhataAccount *accountAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<KhataAccount> m_rows;
};

// Ledger lines, newest first. The customer view hides the "By" column.
class KhataEntryTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit KhataEntryTableModel(bool showAuthor, QObject *parent = nullptr);

    void setEntries(QVector<KhataEntry> entries);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    bool m_showAuthor;
    QVector<KhataEntry> m_rows;
};

} // namespace shophub::ui
