#include "ui/dialogs/KhataDialogs.h"

#include "services/AppContext.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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

QString owesText(Money balance)
{
    if (balance.isNegative())
        return QStringLiteral("has an advance of %1").arg((Money() - balance).toString());
    return QStringLiteral("owes %1").arg(balance.toString());
}

} // namespace

KhataDialog::KhataDialog(UiContext &ui, const KhataAccount &account, Mode mode, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_account(account), m_mode(mode)
{
    const QString title = mode == Mode::SetLimit        ? QStringLiteral("Set credit limit")
                          : mode == Mode::RecordPayment ? QStringLiteral("Record payment")
                                                        : QStringLiteral("Adjust khata");
    setWindowTitle(title);
    setMinimumWidth(480);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    v->addWidget(makeLabel(title, "h2"));
    QString summary = QStringLiteral("%1  (%2)\nCredit limit %3  ·  %4")
                          .arg(account.customerName, account.email,
                               account.creditLimit.isZero() ? QStringLiteral("not set") : account.creditLimit.toString(),
                               owesText(account.balance));
    if (account.qistBalance > Money())
        summary += QStringLiteral("  (%1 of it on qist, paid on the Qist page)").arg(account.qistBalance.toString());
    auto *who = makeLabel(summary, "muted");
    who->setWordWrap(true);
    v->addWidget(who);

    auto *form = new QFormLayout();
    form->setVerticalSpacing(10);

    switch (mode) {
    case Mode::SetLimit:
        m_amount = field(QStringLiteral("e.g. 25000  (0 closes the khata)"));
        if (!account.creditLimit.isZero())
            m_amount->setText(account.creditLimit.toPlainString());
        form->addRow(makeFormLabel(QStringLiteral("Credit limit (Rs)")), m_amount);
        break;
    case Mode::RecordPayment:
        m_amount = field(QStringLiteral("Amount received"));
        if (account.creditUsed() > Money())
            m_amount->setText(account.creditUsed().toPlainString()); // the khata part; qist is paid on the Qist page
        m_choice = new QComboBox();
        m_choice->setEditable(true);
        m_choice->addItems({QStringLiteral("Cash"), QStringLiteral("Easypaisa"), QStringLiteral("JazzCash"),
                            QStringLiteral("Bank transfer"), QStringLiteral("Cheque")});
        m_choice->setMinimumHeight(34);
        m_note = field(QStringLiteral("Optional, e.g. receipt or transaction ID"));
        form->addRow(makeFormLabel(QStringLiteral("Amount (Rs)")), m_amount);
        form->addRow(makeFormLabel(QStringLiteral("Paid by")), m_choice);
        form->addRow(makeFormLabel(QStringLiteral("Reference")), m_note);
        break;
    case Mode::Adjust:
        m_choice = new QComboBox();
        m_choice->addItem(QStringLiteral("Reduce what they owe (discount, write-off, correction)"), -1);
        m_choice->addItem(QStringLiteral("Increase what they owe (correction)"), 1);
        m_choice->setMinimumHeight(34);
        m_amount = field(QStringLiteral("Amount"));
        m_note = field(QStringLiteral("Why? (required)"));
        form->addRow(makeFormLabel(QStringLiteral("Change")), m_choice);
        form->addRow(makeFormLabel(QStringLiteral("Amount (Rs)")), m_amount);
        form->addRow(makeFormLabel(QStringLiteral("Reason")), m_note);
        break;
    }
    v->addLayout(form);

    if (mode != Mode::SetLimit) {
        auto *hint = makeLabel(QStringLiteral("Khata entries are permanent. A mistake is fixed with another adjustment."),
                               "small");
        hint->setWordWrap(true);
        v->addWidget(hint);
    }

    m_preview = makeLabel(QString(), "h3");
    v->addWidget(m_preview);
    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    v->addWidget(m_error);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = makeButton(QStringLiteral("Cancel"), "ghost");
    auto *save = makeButton(mode == Mode::SetLimit ? QStringLiteral("Save limit")
                            : mode == Mode::RecordPayment ? QStringLiteral("Record payment")
                                                          : QStringLiteral("Save adjustment"),
                            "primary");
    save->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    v->addLayout(buttons);

    connect(m_amount, &QLineEdit::textChanged, this, &KhataDialog::updatePreview);
    if (m_choice && mode == Mode::Adjust)
        connect(m_choice, qOverload<int>(&QComboBox::currentIndexChanged), this, &KhataDialog::updatePreview);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &KhataDialog::save);
    updatePreview();
    m_amount->setFocus();
    m_amount->selectAll();
}

std::optional<Money> KhataDialog::amount() const
{
    const QString text = m_amount->text().trimmed();
    if (text.isEmpty())
        return std::nullopt;
    return Money::parse(text);
}

void KhataDialog::updatePreview()
{
    m_error->clear();
    const auto value = amount();
    if (!value) {
        m_preview->clear();
        return;
    }
    switch (m_mode) {
    case Mode::SetLimit:
        m_preview->setText(value->isZero() ? QStringLiteral("Khata will be closed for new orders.")
                                           : QStringLiteral("They can buy up to %1 on khata.")
                                                 .arg((*value > m_account.creditUsed() ? *value - m_account.creditUsed() : Money()).toString()));
        break;
    case Mode::RecordPayment:
        m_preview->setText(QStringLiteral("New balance: %1").arg((m_account.balance - *value).toString()));
        break;
    case Mode::Adjust: {
        const Money change = m_choice->currentData().toInt() < 0 ? Money() - *value : *value;
        m_preview->setText(QStringLiteral("New balance: %1").arg((m_account.balance + change).toString()));
        break;
    }
    }
}

void KhataDialog::save()
{
    const auto value = amount();
    if (!value) {
        m_error->setText(QStringLiteral("Enter an amount in rupees, for example 2500 or 2500.50."));
        return;
    }

    Status status;
    QString done;
    switch (m_mode) {
    case Mode::SetLimit:
        status = m_ui.app.khata().setLimit(m_ui.session, m_account.userId, *value);
        done = value->isZero() ? QStringLiteral("Khata closed for %1").arg(m_account.customerName)
                               : QStringLiteral("%1 can now buy up to %2 on khata").arg(m_account.customerName, value->toString());
        break;
    case Mode::RecordPayment: {
        QString note = m_choice->currentText().trimmed();
        if (!m_note->text().trimmed().isEmpty())
            note += QStringLiteral(" - ") + m_note->text().trimmed();
        auto r = m_ui.app.khata().recordPayment(m_ui.session, m_account.userId, *value, note);
        status = r.status();
        if (r)
            done = QStringLiteral("Payment recorded. %1 now %2").arg(m_account.customerName, owesText(r.value()));
        break;
    }
    case Mode::Adjust: {
        const Money change = m_choice->currentData().toInt() < 0 ? Money() - *value : *value;
        auto r = m_ui.app.khata().adjust(m_ui.session, m_account.userId, change, m_note->text());
        status = r.status();
        if (r)
            done = QStringLiteral("Khata adjusted. %1 now %2").arg(m_account.customerName, owesText(r.value()));
        break;
    }
    }

    if (!status) {
        m_error->setText(status.error());
        return;
    }
    emit m_ui.events->khataChanged();
    emit m_ui.events->toast(done);
    accept();
}

} // namespace shophub::ui
