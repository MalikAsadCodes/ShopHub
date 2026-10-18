#pragma once

#include "ui/pages/Page.h"

#include "domain/Types.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTimer;

namespace shophub::ui {

class EmptyState;
class OrderTableModel;
class ProductGrid;

class HomePage : public Page
{
    Q_OBJECT
public:
    explicit HomePage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;

private:
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
};

class ShopPage : public Page
{
    Q_OBJECT
public:
    explicit ShopPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;
    void applySearch(const QString &text) override;
    void showCategory(qint64 categoryId, const QString &text);

private:
    void reloadFilters();
    void runSearch(bool append);
    void resetFilters();

    QLineEdit *m_search = nullptr;
    QComboBox *m_category = nullptr;
    QComboBox *m_brand = nullptr;
    QLineEdit *m_minPrice = nullptr;
    QLineEdit *m_maxPrice = nullptr;
    QCheckBox *m_inStock = nullptr;
    QComboBox *m_sort = nullptr;
    QLabel *m_count = nullptr;
    QLabel *m_filterError = nullptr;
    ProductGrid *m_grid = nullptr;
    QPushButton *m_loadMore = nullptr;
    EmptyState *m_empty = nullptr;
    QStackedWidget *m_results = nullptr;
    QTimer *m_debounce = nullptr;
    int m_loaded = 0;
    bool m_updatingFilters = false;
};

class CartPage : public Page
{
    Q_OBJECT
public:
    explicit CartPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;

private:
    QWidget *buildRow(const CartItem &item);
    void openCheckout();

    QStackedWidget *m_stack = nullptr;
    QVBoxLayout *m_rows = nullptr;
    QVBoxLayout *m_summary = nullptr;
    QWidget *m_rowsHost = nullptr;
    QWidget *m_summaryHost = nullptr;
};

class MyOrdersPage : public Page
{
    Q_OBJECT
public:
    explicit MyOrdersPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;

private:
    void openSelected();

    OrderTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QStackedWidget *m_stack = nullptr;
};

} // namespace shophub::ui
