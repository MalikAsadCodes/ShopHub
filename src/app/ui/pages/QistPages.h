#pragma once

#include "ui/pages/Page.h"

#include "domain/Types.h"

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
class QistAccountTableModel;
class QistScheduleTableModel;
class StatCard;

// Admin: every qist with paid, left and the next due date (overdue first),
// the selected one's schedule, recording installments and managing plans.
class QistPage : public Page
{
    Q_OBJECT
public:
    explicit QistPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;
    void applySearch(const QString &text) override;

private:
    const QistAccount *selected() const;
    void loadSchedule();
    void recordPayment();

    QLineEdit *m_search = nullptr;
    QComboBox *m_show = nullptr;
    QLabel *m_summary = nullptr;
    QistAccountTableModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_scheduleTitle = nullptr;
    QistScheduleTableModel *m_schedule = nullptr;
    QTableView *m_scheduleTable = nullptr;
    QPushButton *m_pay = nullptr;
    QTimer *m_debounce = nullptr;
};

// Customer: what I am buying on qist, what is left and when the next installment is due.
class MyQistPage : public Page
{
    Q_OBJECT
public:
    explicit MyQistPage(UiContext &ui, QWidget *parent = nullptr);
    void refresh() override;

private:
    const QistAccount *selected() const;
    void loadSchedule();

    QStackedWidget *m_stack = nullptr;
    EmptyState *m_empty = nullptr;
    StatCard *m_left = nullptr;
    StatCard *m_next = nullptr;
    StatCard *m_overdue = nullptr;
    QistAccountTableModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_scheduleTitle = nullptr;
    QistScheduleTableModel *m_schedule = nullptr;
    QTableView *m_scheduleTable = nullptr;
};

} // namespace shophub::ui
