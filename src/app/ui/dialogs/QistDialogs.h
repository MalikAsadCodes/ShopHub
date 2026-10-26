#pragma once

#include "domain/Types.h"
#include "ui/UiCommon.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace shophub::ui {

// Admin: the qist plans customers can choose at checkout. Add, edit, switch off.
class QistPlansDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QistPlansDialog(UiContext &ui, QWidget *parent = nullptr);

    // For tests: fill the form and save it as a new plan (or over the selected one).
    void fillForm(const QString &name, int months, const QString &markupPercent, const QString &downPercent);
    void startNewPlan();
    void savePlan();
    int planCount() const { return int(m_plans.size()); }

private:
    void reload(qint64 selectId = 0);
    void showSelected();
    void toggleSelected();
    const QistPlan *selected() const;

    UiContext &m_ui;
    QVector<QistPlan> m_plans;
    qint64 m_editingId = 0; // 0 = the form makes a new plan
    QTableWidget *m_table = nullptr;
    QLabel *m_formTitle = nullptr;
    QLineEdit *m_name = nullptr;
    QSpinBox *m_months = nullptr;
    QLineEdit *m_markup = nullptr;
    QLineEdit *m_down = nullptr;
    QLabel *m_example = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_toggle = nullptr;
    QPushButton *m_save = nullptr;
};

// Admin: money received for one qist, paid towards the oldest installment first.
class QistPaymentDialog : public QDialog
{
    Q_OBJECT
public:
    QistPaymentDialog(UiContext &ui, const QistAccount &account, QWidget *parent = nullptr);

    // For tests.
    void setAmount(const QString &rupees);
    void save();

private:
    std::optional<Money> amount() const;
    void updatePreview();

    UiContext &m_ui;
    QistAccount m_account;
    QLineEdit *m_amount = nullptr;
    QComboBox *m_paidBy = nullptr;
    QLineEdit *m_reference = nullptr;
    QLabel *m_preview = nullptr;
    QLabel *m_error = nullptr;
};

} // namespace shophub::ui
