#pragma once

#include "ui/pages/Page.h"

#include "domain/Types.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QTableWidget;
class QTimer;
class QVBoxLayout;

namespace shophub::ui {

class MovementTableModel;
class OrderTableModel;
class ProductTableModel;
class SalesChart;
class StatCard;

class DashboardPage : public Page
{
    Q_OBJECT
public:
    explicit DashboardPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;

private:
    StatCard *m_sales = nullptr;
    StatCard *m_orders = nullptr;
    StatCard *m_aov = nullptr;
    StatCard *m_open = nullptr;
    StatCard *m_lowStock = nullptr;
    StatCard *m_customers = nullptr;
    SalesChart *m_chart = nullptr;
    QLabel *m_chartTotal = nullptr;
    QVBoxLayout *m_topProducts = nullptr;
    QTableWidget *m_recent = nullptr;
    QVector<qint64> m_recentIds;
};

class AdminProductsPage : public Page
{
    Q_OBJECT
public:
    explicit AdminProductsPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;
    void applySearch(const QString &text) override;

private:
    const Product *selected() const;
    void updateButtons();
    void editSelected();
    void toggleSelected();

    QLineEdit *m_search = nullptr;
    QComboBox *m_category = nullptr;
    QCheckBox *m_showInactive = nullptr;
    QLabel *m_count = nullptr;
    ProductTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_toggle = nullptr;
    QPushButton *m_preview = nullptr;
    QTimer *m_debounce = nullptr;
};

class InventoryPage : public Page
{
    Q_OBJECT
public:
    explicit InventoryPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;
    void applySearch(const QString &text) override;

private:
    const Product *selected() const;
    void loadMovements();
    void adjustSelected();

    QLineEdit *m_search = nullptr;
    QCheckBox *m_lowOnly = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_movementsTitle = nullptr;
    ProductTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    MovementTableModel *m_movements = nullptr;
    QTableView *m_movementTable = nullptr;
    QPushButton *m_adjust = nullptr;
    QTimer *m_debounce = nullptr;
};

class AdminOrdersPage : public Page
{
    Q_OBJECT
public:
    explicit AdminOrdersPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;
    void applySearch(const QString &text) override;

private:
    const Order *selected() const;
    void updateButtons();
    void openSelected();
    void advanceSelected();
    void cancelSelected();

    QComboBox *m_status = nullptr;
    QLineEdit *m_search = nullptr;
    QLabel *m_count = nullptr;
    OrderTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QPushButton *m_view = nullptr;
    QPushButton *m_advance = nullptr;
    QPushButton *m_cancel = nullptr;
    QTimer *m_debounce = nullptr;
};

} // namespace shophub::ui
