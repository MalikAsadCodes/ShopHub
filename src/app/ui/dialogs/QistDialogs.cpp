#include "ui/dialogs/QistDialogs.h"

#include "services/AppContext.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace shophub::ui {

namespace {

QLineEdit *field(const QString &placeholder)
{
    auto *e = new QLineEdit();
    e->setPlaceholderText(placeholder);
    e->setAccessibleName(placeholder);
    e->setMinimumHeight(34);
    return e;
}

QString dayText(const QDate &date)
{
    return QLocale(QLocale::English).toString(date, QStringLiteral("d MMM yyyy"));
}

} // namespace

// ====================================================================== Plans
QistPlansDialog::QistPlansDialog(UiContext &ui, QWidget *parent)
    : QDialog(parent), m_ui(ui)
{
    setWindowTitle(QStringLiteral("Qist plans"));
    setMinimumSize(720, 560);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    v->addWidget(makeLabel(QStringLiteral("Qist plans"), "h2"));
    auto *intro = makeLabel(QStringLiteral("Customers choose one of these at checkout. Changing a plan never changes "
                                           "a qist that was already sold; switch a plan off to stop offering it."),
                            "small");
    intro->setWordWrap(true);
    v->addWidget(intro);

    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Plan"), QStringLiteral("Months"), QStringLiteral("Markup"),
                                        QStringLiteral("Min. down"), QStringLiteral("Sold"), QStringLiteral("Status")});
    setupTable(m_table);
    m_table->setSortingEnabled(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setMinimumHeight(170);
    v->addWidget(m_table, 1);

    m_formTitle = makeLabel(QString(), "h3");
    v->addWidget(m_formTitle);
    m_name = field(QStringLiteral("Plan name, e.g. 6 months"));
    m_months = new QSpinBox();
    m_months->setRange(qist::kMinMonths, qist::kMaxMonths);
    m_months->setValue(6);
    m_months->setSuffix(QStringLiteral(" months"));
    m_months->setMinimumHeight(34);
    m_months->setAccessibleName(QStringLiteral("Months"));
    m_markup = field(QStringLiteral("Markup %, e.g. 10 (0 = none)"));
    m_down = field(QStringLiteral("Minimum down payment %, e.g. 20"));
    auto *form = new QFormLayout();
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);
    form->addRow(makeFormLabel(QStringLiteral("Name")), m_name);
    form->addRow(makeFormLabel(QStringLiteral("Installments")), m_months);
    form->addRow(makeFormLabel(QStringLiteral("Markup (%)")), m_markup);
    form->addRow(makeFormLabel(QStringLiteral("Down payment (%)")), m_down);
    v->addLayout(form);
    m_example = makeLabel(QString(), "small");
    m_example->setWordWrap(true);
    v->addWidget(m_example);
    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    v->addWidget(m_error);

    auto *buttons = new QHBoxLayout();
    auto *newPlan = makeButton(QStringLiteral("New plan"), "ghost");
    m_toggle = makeButton(QStringLiteral("Switch off"));
    buttons->addWidget(newPlan);
    buttons->addWidget(m_toggle);
    buttons->addStretch();
    auto *close = makeButton(QStringLiteral("Close"), "ghost");
    m_save = makeButton(QStringLiteral("Save plan"), "primary");
    m_save->setDefault(true);
    buttons->addWidget(close);
    buttons->addWidget(m_save);
    v->addLayout(buttons);

    connect(newPlan, &QPushButton::clicked, this, &QistPlansDialog::startNewPlan);
    connect(m_toggle, &QPushButton::clicked, this, &QistPlansDialog::toggleSelected);
    connect(m_save, &QPushButton::clicked, this, &QistPlansDialog::savePlan);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &QistPlansDialog::showSelected);

    // Live example: what a Rs 10,000 order would cost on the plan in the form.
    auto updateExample = [this] {
        const auto markup = pricetier::parsePercent(m_markup->text());
        const auto down = pricetier::parsePercent(m_down->text());
        if (!markup || !down) {
            m_example->setText(QStringLiteral("Enter the markup and the down payment as percentages, e.g. 10 and 20."));
            return;
        }
        auto q = qist::quote(QistTerms{m_months->value(), *markup, *down}, Money::fromRupees(10000), std::nullopt,
                             QDate::currentDate());
        if (!q) {
            m_example->setText(q.error());
            return;
        }
        const QistQuote &v = q.value();
        const QString monthly = v.lastMonthly == v.monthly
                                    ? QStringLiteral("%1 × %2").arg(v.terms.months).arg(v.monthly.toString())
                                    : QStringLiteral("%1 × %2 (last %3)")
                                          .arg(v.terms.months)
                                          .arg(v.monthly.toString(), v.lastMonthly.toString());
        m_example->setText(QStringLiteral("Example, a Rs 10,000 order: %1 down, then %2. Qist price %3.")
                               .arg(v.downPayment.toString(), monthly, v.qistPrice.toString()));
    };
    connect(m_markup, &QLineEdit::textChanged, this, updateExample);
    connect(m_down, &QLineEdit::textChanged, this, updateExample);
    connect(m_months, qOverload<int>(&QSpinBox::valueChanged), this, updateExample);

    reload();
    startNewPlan();
}

const QistPlan *QistPlansDialog::selected() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_plans.size() && m_table->selectionModel()->hasSelection() ? &m_plans[row] : nullptr;
}

void QistPlansDialog::reload(qint64 selectId)
{
    auto plans = m_ui.app.qist().plans(m_ui.session, true);
    if (!plans) {
        m_error->setText(plans.error());
        return;
    }
    m_plans = plans.value();
    m_table->blockSignals(true);
    m_table->clearSelection();
    m_table->setRowCount(m_plans.size());
    int selectRow = -1;
    for (int r = 0; r < m_plans.size(); ++r) {
        const QistPlan &p = m_plans[r];
        const QStringList cells = {p.name, QString::number(p.terms.months),
                                   pricetier::percentText(p.terms.markupBp) + QLatin1Char('%'),
                                   pricetier::percentText(p.terms.minDownBp) + QLatin1Char('%'),
                                   QString::number(p.qistCount),
                                   p.active ? QStringLiteral("On offer") : QStringLiteral("Switched off")};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            if (c >= 1 && c <= 4)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(r, c, item);
        }
        if (p.id == selectId)
            selectRow = r;
    }
    m_table->blockSignals(false);
    if (selectRow >= 0)
        m_table->selectRow(selectRow);
    m_toggle->setEnabled(selected());
}

void QistPlansDialog::startNewPlan()
{
    m_table->clearSelection();
    m_editingId = 0;
    m_formTitle->setText(QStringLiteral("New plan"));
    m_name->clear();
    m_months->setValue(6);
    m_markup->setText(QStringLiteral("10"));
    m_down->setText(QStringLiteral("20"));
    m_error->clear();
    m_toggle->setEnabled(false);
    m_toggle->setText(QStringLiteral("Switch off"));
    m_name->setFocus();
}

void QistPlansDialog::showSelected()
{
    const QistPlan *p = selected();
    m_toggle->setEnabled(p);
    if (!p)
        return;
    m_editingId = p->id;
    m_formTitle->setText(QStringLiteral("Edit \"%1\"").arg(p->name));
    m_name->setText(p->name);
    m_months->setValue(p->terms.months);
    m_markup->setText(pricetier::percentText(p->terms.markupBp));
    m_down->setText(pricetier::percentText(p->terms.minDownBp));
    m_toggle->setText(p->active ? QStringLiteral("Switch off") : QStringLiteral("Offer again"));
    m_error->clear();
}

void QistPlansDialog::fillForm(const QString &name, int months, const QString &markupPercent,
                               const QString &downPercent)
{
    m_name->setText(name);
    m_months->setValue(months);
    m_markup->setText(markupPercent);
    m_down->setText(downPercent);
}

void QistPlansDialog::savePlan()
{
    const auto markup = pricetier::parsePercent(m_markup->text());
    const auto down = pricetier::parsePercent(m_down->text());
    if (!markup || !down) {
        m_error->setText(QStringLiteral("Enter the markup and the down payment as percentages, e.g. 10 and 20 (0 is fine)."));
        return;
    }
    QistPlanInput input{m_name->text(), QistTerms{m_months->value(), *markup, *down}};
    qint64 id = m_editingId;
    Status st = Status::ok();
    if (m_editingId > 0) {
        st = m_ui.app.qist().updatePlan(m_ui.session, m_editingId, input);
    } else {
        auto created = m_ui.app.qist().createPlan(m_ui.session, input);
        st = created.status();
        if (created)
            id = created.value();
    }
    if (!st) {
        m_error->setText(st.error());
        return;
    }
    emit m_ui.events->toast(QStringLiteral("Qist plan saved"));
    reload(id);
}

void QistPlansDialog::toggleSelected()
{
    const QistPlan *p = selected();
    if (!p)
        return;
    const qint64 id = p->id;
    const Status st = m_ui.app.qist().setPlanActive(m_ui.session, id, !p->active);
    if (!st) {
        m_error->setText(st.error());
        return;
    }
    reload(id);
}

// ====================================================================== Record installment
QistPaymentDialog::QistPaymentDialog(UiContext &ui, const QistAccount &account, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_account(account)
{
    setWindowTitle(QStringLiteral("Record installment"));
    setMinimumWidth(500);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    v->addWidget(makeLabel(QStringLiteral("Record installment"), "h2"));

    QString summary = QStringLiteral("%1  ·  order %2  ·  %3\nQist price %4  ·  paid %5  ·  left %6")
                          .arg(account.customerName, account.orderNumber, account.planName,
                               account.qistPrice.toString(), account.paid.toString(), account.left().toString());
    if (const auto next = account.nextDue())
        summary += QStringLiteral("\nNext: %1, %2 due %3")
                       .arg(qist::installmentName(next->installment.number, account.terms.months),
                            next->remaining().toString(), dayText(next->installment.dueDate));
    auto *who = makeLabel(summary, "muted");
    who->setWordWrap(true);
    v->addWidget(who);
    if (account.isOverdue()) {
        auto *late = makeLabel(QStringLiteral("Overdue: %1").arg(account.overdueAmount().toString()), "error");
        v->addWidget(late);
    }

    m_amount = field(QStringLiteral("Amount received"));
    // Suggest what is due: everything overdue, otherwise the next installment.
    Money suggested = account.overdueAmount();
    if (suggested.isZero()) {
        if (const auto next = account.nextDue())
            suggested = next->remaining();
    }
    if (suggested > Money())
        m_amount->setText(suggested.toPlainString());
    m_paidBy = new QComboBox();
    m_paidBy->setEditable(true);
    m_paidBy->addItems({QStringLiteral("Cash"), QStringLiteral("Easypaisa"), QStringLiteral("JazzCash"),
                        QStringLiteral("Bank transfer"), QStringLiteral("Cheque")});
    m_paidBy->setMinimumHeight(34);
    m_reference = field(QStringLiteral("Optional, e.g. receipt or transaction ID"));
    auto *form = new QFormLayout();
    form->setVerticalSpacing(10);
    form->addRow(makeFormLabel(QStringLiteral("Amount (Rs)")), m_amount);
    form->addRow(makeFormLabel(QStringLiteral("Paid by")), m_paidBy);
    form->addRow(makeFormLabel(QStringLiteral("Reference")), m_reference);
    v->addLayout(form);

    auto *hint = makeLabel(QStringLiteral("Payments go to the oldest installment first and are kept in the customer's khata."),
                           "small");
    hint->setWordWrap(true);
    v->addWidget(hint);
    m_preview = makeLabel(QString(), "h3");
    m_preview->setWordWrap(true);
    v->addWidget(m_preview);
    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    v->addWidget(m_error);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = makeButton(QStringLiteral("Cancel"), "ghost");
    auto *save = makeButton(QStringLiteral("Record installment"), "primary");
    save->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    v->addLayout(buttons);

    connect(m_amount, &QLineEdit::textChanged, this, &QistPaymentDialog::updatePreview);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &QistPaymentDialog::save);
    updatePreview();
    m_amount->setFocus();
    m_amount->selectAll();
}

std::optional<Money> QistPaymentDialog::amount() const
{
    const QString text = m_amount->text().trimmed();
    if (text.isEmpty())
        return std::nullopt;
    return Money::parse(text);
}

void QistPaymentDialog::setAmount(const QString &rupees)
{
    m_amount->setText(rupees);
}

void QistPaymentDialog::updatePreview()
{
    m_error->clear();
    const auto value = amount();
    if (!value || value->isZero()) {
        m_preview->clear();
        return;
    }
    const Money left = m_account.left();
    if (*value > left) {
        m_preview->setText(QStringLiteral("Only %1 is left to pay.").arg(left.toString()));
        return;
    }
    QString text = qist::describePayment(m_account.schedule, m_account.paid, *value);
    text += *value == left ? QStringLiteral("  ·  the qist will be paid off")
                           : QStringLiteral("  ·  left after this: %1").arg((left - *value).toString());
    m_preview->setText(text);
}

void QistPaymentDialog::save()
{
    const auto value = amount();
    if (!value) {
        m_error->setText(QStringLiteral("Enter an amount in rupees, for example 2500 or 2500.50."));
        return;
    }
    QString note = m_paidBy->currentText().trimmed();
    if (!m_reference->text().trimmed().isEmpty())
        note += QStringLiteral(" - ") + m_reference->text().trimmed();
    auto result = m_ui.app.qist().recordPayment(m_ui.session, m_account.id, *value, note);
    if (!result) {
        m_error->setText(result.error());
        return;
    }
    const QistAccount &after = result.value();
    emit m_ui.events->khataChanged();
    emit m_ui.events->ordersChanged();
    emit m_ui.events->toast(after.state == QistState::Completed
                                ? QStringLiteral("Qist paid off. Order %1 is now paid.").arg(after.orderNumber)
                                : QStringLiteral("Installment recorded. %1 left on %2's qist.")
                                      .arg(after.left().toString(), after.customerName));
    accept();
}

} // namespace shophub::ui
