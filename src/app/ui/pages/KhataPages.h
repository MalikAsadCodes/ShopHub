#pragma once

#include "ui/pages/Page.h"

#include "domain/Types.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTimer;

namespace shophub::ui {

class KhataAccountTableModel;
class KhataEntryTableModel;
class StatCard;

// Admin: every customer's credit limit and balance, plus the selected customer's ledger.
class KhataPage : public Page
{
    Q_OBJECT
public:
    explicit KhataPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;
    void applySearch(const QString &text) override;

private:
    const KhataAccount *selected() const;
    void loadLedger();
    void openDialog(int mode);
    void toggleQist();

    QLineEdit *m_search = nullptr;
    QCheckBox *m_owingOnly = nullptr;
    QLabel *m_summary = nullptr;
    KhataAccountTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_ledgerTitle = nullptr;
    KhataEntryTableModel *m_entries = nullptr;
    QTableView *m_ledger = nullptr;
    QPushButton *m_limit = nullptr;
    QPushButton *m_payment = nullptr;
    QPushButton *m_adjust = nullptr;
    QPushButton *m_qist = nullptr;
    QTimer *m_debounce = nullptr;
};

// Customer: what I owe, my limit, and my khata history.
class MyKhataPage : public Page
{
    Q_OBJECT
public:
    explicit MyKhataPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;

private:
    QStackedWidget *m_stack = nullptr;
    StatCard *m_owe = nullptr;
    StatCard *m_limit = nullptr;
    StatCard *m_available = nullptr;
    KhataEntryTableModel *m_entries = nullptr;
    QTableView *m_ledger = nullptr;
};

} // namespace shophub::ui
