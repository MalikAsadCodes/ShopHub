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
    enum Column { Sku, Name, Category, Brand, Price, Cost, Stock, LowAt, StockState, Status, Wholesale, Qist };

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

// Qists with what is paid, what is left and the next due date. The admin view
// adds the customer; the customer view leaves it out.
class QistAccountTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit QistAccountTableModel(bool showCustomer, QObject *parent = nullptr);

    void setAccounts(QVector<QistAccount> accounts);
    const QistAccount *accountAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    enum Column { Customer, Order, Plan, Price, Paid, Left, NextDue, State };
    QVector<Column> m_columns;
    QVector<QistAccount> m_rows;
};

// One qist's schedule: down payment and monthly installments.
class QistScheduleTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit QistScheduleTableModel(QObject *parent = nullptr);

    void setAccount(const QistAccount &account); // an empty account clears the table
    void clear();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<InstallmentStatus> m_rows;
    int m_months = 0;
    QistState m_state = QistState::Active;
};

} // namespace shophub::ui
