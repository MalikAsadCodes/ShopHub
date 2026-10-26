#include "ui/pages/KhataPages.h"

#include "services/AppContext.h"
#include "ui/dialogs/KhataDialogs.h"
#include "ui/models/TableModels.h"
#include "ui/widgets/Widgets.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>

namespace shophub::ui {

// ====================================================================== Khata (admin)
KhataPage::KhataPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    auto *actions = new QWidget();
    auto *ah = new QHBoxLayout(actions);
    ah->setContentsMargins(0, 0, 0, 0);
    ah->setSpacing(8);
    m_limit = makeButton(QStringLiteral("Set credit limit..."));
    m_adjust = makeButton(QStringLiteral("Adjust..."), "ghost");
    m_payment = makeButton(QStringLiteral("Record payment..."), "primary");
    m_qist = makeButton(QStringLiteral("Allow qist"));
    m_limit->setToolTip(QStringLiteral("How much this customer may owe at most. 0 closes the khata."));
    m_adjust->setToolTip(QStringLiteral("Discount, write-off or correction, with a reason"));
    m_qist->setToolTip(QStringLiteral("Let this customer buy in monthly installments (qist). Existing qists are not affected."));
    ah->addWidget(m_qist);
    ah->addWidget(m_limit);
    ah->addWidget(m_adjust);
    ah->addWidget(m_payment);
    addHeader(QStringLiteral("Khata"),
              QStringLiteral("Credit for trusted customers. Every entry is kept; mistakes are fixed with an adjustment."),
              actions);

    auto *toolbar = new QHBoxLayout();
    m_search = new QLineEdit();
    m_search->setPlaceholderText(QStringLiteral("Search by customer name or email"));
    m_search->setAccessibleName(m_search->placeholderText());
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(280);
    m_owingOnly = new QCheckBox(QStringLiteral("Only customers who owe"));
    toolbar->addWidget(m_search);
    toolbar->addWidget(m_owingOnly);
    toolbar->addStretch();
    m_summary = makeLabel(QString(), "muted");
    toolbar->addWidget(m_summary);
    m_layout->addLayout(toolbar);

    m_model = new KhataAccountTableModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(kSortRole);
    m_table = new QTableView();
    m_table->setModel(m_proxy);
    setupTable(m_table);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->sortByColumn(3, Qt::DescendingOrder); // biggest balance first
    m_layout->addWidget(m_table, 3);

    m_ledgerTitle = makeLabel(QStringLiteral("Ledger"), "h2");
    m_layout->addWidget(m_ledgerTitle);
    m_entries = new KhataEntryTableModel(true, this);
    m_ledger = new QTableView();
    m_ledger->setModel(m_entries);
    setupTable(m_ledger);
    m_ledger->setSortingEnabled(false);
    m_ledger->setSelectionMode(QAbstractItemView::NoSelection);
    m_ledger->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ledger->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_layout->addWidget(m_ledger, 2);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(250);
    connect(m_debounce, &QTimer::timeout, this, &KhataPage::refresh);
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_owingOnly, &QCheckBox::toggled, this, &KhataPage::refresh);
    connect(m_limit, &QPushButton::clicked, this, [this] { openDialog(int(KhataDialog::Mode::SetLimit)); });
    connect(m_payment, &QPushButton::clicked, this, [this] { openDialog(int(KhataDialog::Mode::RecordPayment)); });
    connect(m_adjust, &QPushButton::clicked, this, [this] { openDialog(int(KhataDialog::Mode::Adjust)); });
    connect(m_qist, &QPushButton::clicked, this, &KhataPage::toggleQist);
    connect(m_table, &QTableView::doubleClicked, this, [this] {
        const KhataAccount *a = selected();
        if (a)
            openDialog(int(a->creditUsed() > Money() ? KhataDialog::Mode::RecordPayment : KhataDialog::Mode::SetLimit));
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &KhataPage::loadLedger);
}

void KhataPage::applySearch(const QString &text)
{
    m_search->setText(text);
    refresh();
}

const KhataAccount *KhataPage::selected() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return m_model->accountAt(m_proxy->mapToSource(idx).row());
}

void KhataPage::refresh()
{
    m_debounce->stop();
    const KhataAccount *current = selected();
    const qint64 keepId = current ? current->userId : 0;

    auto accounts = m_ui.app.khata().accounts(m_ui.session, m_search->text(), m_owingOnly->isChecked());
    if (!accounts) {
        emit m_ui.events->toast(accounts.error());
        return;
    }
    Money owed;
    int owing = 0;
    int open = 0;
    for (const KhataAccount &a : accounts.value()) {
        if (a.balance > Money()) {
            owed += a.balance;
            ++owing;
        }
        if (!a.creditLimit.isZero())
            ++open;
    }
    m_model->setAccounts(accounts.value());
    m_summary->setText(QStringLiteral("%1 owed by %2 customer(s)  ·  %3 khata(s) open")
                           .arg(owed.toString()).arg(owing).arg(open));

    int row = m_proxy->rowCount() > 0 ? 0 : -1;
    for (int r = 0; r < m_proxy->rowCount(); ++r) {
        if (m_proxy->data(m_proxy->index(r, 0), kIdRole).toLongLong() == keepId) {
            row = r;
            break;
        }
    }
    if (row >= 0)
        m_table->selectRow(row);
    loadLedger();
}

void KhataPage::loadLedger()
{
    const KhataAccount *a = selected();
    m_limit->setEnabled(a);
    m_payment->setEnabled(a && a->creditUsed() > Money());
    m_adjust->setEnabled(a);
    m_qist->setEnabled(a);
    m_qist->setText(a && a->qistApproved ? QStringLiteral("Stop qist") : QStringLiteral("Allow qist"));
    if (!a) {
        m_ledgerTitle->setText(QStringLiteral("Ledger"));
        m_entries->setEntries({});
        return;
    }
    m_ledgerTitle->setText(QStringLiteral("Ledger - %1").arg(a->customerName));
    auto entries = m_ui.app.khata().entries(m_ui.session, a->userId, 500);
    if (entries)
        m_entries->setEntries(entries.value());
    else
        emit m_ui.events->toast(entries.error());
}

void KhataPage::toggleQist()
{
    const KhataAccount *a = selected();
    if (!a)
        return;
    const bool allow = !a->qistApproved;
    const QString name = a->customerName;
    if (!allow && !confirm(this, QStringLiteral("Stop qist"),
                           QStringLiteral("Stop new qists for %1? Qists they already have carry on as agreed.").arg(name),
                           QStringLiteral("Stop qist")))
        return;
    const Status st = m_ui.app.qist().setApproved(m_ui.session, a->userId, allow);
    if (!st) {
        showError(this, st.error());
        return;
    }
    emit m_ui.events->khataChanged();
    emit m_ui.events->toast(allow ? QStringLiteral("%1 can now buy on qist").arg(name)
                                  : QStringLiteral("No new qists for %1").arg(name));
}

void KhataPage::openDialog(int mode)
{
    const KhataAccount *a = selected();
    if (!a)
        return;
    KhataDialog dialog(m_ui, *a, static_cast<KhataDialog::Mode>(mode), window());
    dialog.exec();
}

// ====================================================================== My khata (customer)
MyKhataPage::MyKhataPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    addHeader(QStringLiteral("My khata"),
              QStringLiteral("Buy now and pay the shop later, up to the credit limit the shop has given you."));

    m_stack = new QStackedWidget();
    auto *empty = new EmptyState(QStringLiteral("Khata is not open yet"),
                                 QStringLiteral("Ask the shop to give you a credit limit. Once it is open, choose "
                                                "\"Khata\" at checkout and pay the shop later."),
                                 QStringLiteral("Continue shopping"));
    connect(empty, &EmptyState::actionClicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
    m_stack->addWidget(empty);

    auto *content = new QWidget();
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(16);
    auto *cards = new QGridLayout();
    cards->setHorizontalSpacing(14);
    m_owe = new StatCard(QStringLiteral("You owe"));
    m_limit = new StatCard(QStringLiteral("Credit limit"));
    m_available = new StatCard(QStringLiteral("Available credit"));
    cards->addWidget(m_owe, 0, 0);
    cards->addWidget(m_limit, 0, 1);
    cards->addWidget(m_available, 0, 2);
    v->addLayout(cards);
    v->addWidget(makeLabel(QStringLiteral("History"), "h2"));
    m_entries = new KhataEntryTableModel(false, this);
    m_ledger = new QTableView();
    m_ledger->setModel(m_entries);
    setupTable(m_ledger);
    m_ledger->setSortingEnabled(false);
    m_ledger->setSelectionMode(QAbstractItemView::NoSelection);
    m_ledger->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ledger->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    v->addWidget(m_ledger, 1);
    v->addWidget(makeLabel(QStringLiteral("Pay the shop by cash, Easypaisa, JazzCash or bank transfer; "
                                          "the shop records your payment here."),
                           "small"));
    m_stack->addWidget(content);
    m_layout->addWidget(m_stack, 1);
}

void MyKhataPage::refresh()
{
    auto account = m_ui.app.khata().myAccount(m_ui.session);
    if (!account) {
        emit m_ui.events->toast(account.error());
        return;
    }
    auto entries = m_ui.app.khata().myEntries(m_ui.session);
    if (!entries) {
        emit m_ui.events->toast(entries.error());
        return;
    }
    const KhataAccount &a = account.value();
    const bool open = !a.creditLimit.isZero() || !entries.value().isEmpty();
    m_stack->setCurrentIndex(open ? 1 : 0);
    if (!open)
        return;

    if (a.balance.isNegative())
        m_owe->setValue(Money().toString(), QStringLiteral("The shop holds %1 for you").arg((Money() - a.balance).toString()));
    else if (a.qistBalance > Money())
        m_owe->setValue(a.balance.toString(), QStringLiteral("Includes %1 on qist - see My qist").arg(a.qistBalance.toString()),
                        a.overLimit() ? QStringLiteral("error") : QStringLiteral("small"));
    else
        m_owe->setValue(a.balance.toString(), a.balance.isZero() ? QStringLiteral("All paid up") : QStringLiteral("Please pay the shop"),
                        a.overLimit() ? QStringLiteral("error") : QStringLiteral("small"));
    m_limit->setValue(a.creditLimit.isZero() ? QStringLiteral("Closed") : a.creditLimit.toString(),
                      QStringLiteral("Set by the shop"));
    m_available->setValue(a.available().toString(),
                          a.available().isZero() ? QStringLiteral("Pay some of your khata to buy more")
                                                 : QStringLiteral("You can buy this much on khata"));
    m_entries->setEntries(entries.value());
}

} // namespace shophub::ui
