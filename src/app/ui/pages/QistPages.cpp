#include "ui/pages/QistPages.h"

#include "services/AppContext.h"
#include "ui/dialogs/QistDialogs.h"
#include "ui/models/TableModels.h"
#include "ui/widgets/Widgets.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>

namespace shophub::ui {

namespace {

QString dayText(const QDate &date)
{
    return QLocale(QLocale::English).toString(date, QStringLiteral("d MMM yyyy"));
}

void setupScheduleTable(QTableView *table)
{
    setupTable(table);
    table->setSortingEnabled(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

} // namespace

// ====================================================================== Qist (admin)
QistPage::QistPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    auto *actions = new QWidget();
    auto *ah = new QHBoxLayout(actions);
    ah->setContentsMargins(0, 0, 0, 0);
    ah->setSpacing(8);
    auto *plans = makeButton(QStringLiteral("Plans..."));
    plans->setToolTip(QStringLiteral("The plans customers choose at checkout: months, markup, down payment"));
    m_pay = makeButton(QStringLiteral("Record installment..."), "primary");
    ah->addWidget(plans);
    ah->addWidget(m_pay);
    addHeader(QStringLiteral("Qist"),
              QStringLiteral("Installments: a down payment, then fixed monthly payments. Overdue qists are at the top."),
              actions);

    auto *toolbar = new QHBoxLayout();
    m_search = new QLineEdit();
    m_search->setPlaceholderText(QStringLiteral("Search by customer or order number"));
    m_search->setAccessibleName(m_search->placeholderText());
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(280);
    m_show = new QComboBox();
    m_show->addItem(QStringLiteral("Open qists"), int(QistQuery::Show::Open));
    m_show->addItem(QStringLiteral("Overdue only"), int(QistQuery::Show::Overdue));
    m_show->addItem(QStringLiteral("Completed"), int(QistQuery::Show::Completed));
    m_show->addItem(QStringLiteral("Cancelled"), int(QistQuery::Show::Cancelled));
    m_show->addItem(QStringLiteral("All"), int(QistQuery::Show::All));
    m_show->setMinimumWidth(150);
    toolbar->addWidget(m_search);
    toolbar->addWidget(m_show);
    toolbar->addStretch();
    m_summary = makeLabel(QString(), "muted");
    toolbar->addWidget(m_summary);
    m_layout->addLayout(toolbar);

    m_model = new QistAccountTableModel(true, this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(kSortRole);
    m_table = new QTableView();
    m_table->setModel(m_proxy);
    setupTable(m_table);
    m_table->setSortingEnabled(false); // the service already puts overdue qists first
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_layout->addWidget(m_table, 3);

    m_scheduleTitle = makeLabel(QStringLiteral("Schedule"), "h2");
    m_layout->addWidget(m_scheduleTitle);
    m_schedule = new QistScheduleTableModel(this);
    m_scheduleTable = new QTableView();
    m_scheduleTable->setModel(m_schedule);
    setupScheduleTable(m_scheduleTable);
    m_layout->addWidget(m_scheduleTable, 2);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(250);
    connect(m_debounce, &QTimer::timeout, this, &QistPage::refresh);
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));
    connect(m_show, qOverload<int>(&QComboBox::activated), this, &QistPage::refresh);
    connect(plans, &QPushButton::clicked, this, [this] {
        QistPlansDialog dialog(m_ui, window());
        dialog.exec();
        refresh();
    });
    connect(m_pay, &QPushButton::clicked, this, &QistPage::recordPayment);
    connect(m_table, &QTableView::doubleClicked, this, &QistPage::recordPayment);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &QistPage::loadSchedule);
}

void QistPage::applySearch(const QString &text)
{
    m_search->setText(text);
    refresh();
}

const QistAccount *QistPage::selected() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return m_model->accountAt(m_proxy->mapToSource(idx).row());
}

void QistPage::refresh()
{
    m_debounce->stop();
    const QistAccount *current = selected();
    const qint64 keepId = current ? current->id : 0;

    QistQuery query;
    query.show = static_cast<QistQuery::Show>(m_show->currentData().toInt());
    query.search = m_search->text();
    auto accounts = m_ui.app.qist().accounts(m_ui.session, query);
    if (!accounts) {
        emit m_ui.events->toast(accounts.error());
        return;
    }
    Money left;
    int overdue = 0;
    for (const QistAccount &a : accounts.value()) {
        left += a.left();
        if (a.isOverdue())
            ++overdue;
    }
    m_model->setAccounts(accounts.value());
    m_summary->setText(QStringLiteral("%1 qist(s)  ·  %2 to collect  ·  %3 overdue")
                           .arg(accounts.value().size())
                           .arg(left.toString())
                           .arg(overdue));

    int row = m_proxy->rowCount() > 0 ? 0 : -1;
    for (int r = 0; r < m_proxy->rowCount(); ++r) {
        if (m_proxy->data(m_proxy->index(r, 0), kIdRole).toLongLong() == keepId) {
            row = r;
            break;
        }
    }
    if (row >= 0)
        m_table->selectRow(row);
    loadSchedule();
}

void QistPage::loadSchedule()
{
    const QistAccount *a = selected();
    m_pay->setEnabled(a && a->state == QistState::Active);
    if (!a) {
        m_scheduleTitle->setText(QStringLiteral("Schedule"));
        m_schedule->clear();
        return;
    }
    QString title = QStringLiteral("Schedule - %1, order %2 (%3)").arg(a->customerName, a->orderNumber, a->statusText());
    if (a->isOverdue())
        title += QStringLiteral(": %1 overdue").arg(a->overdueAmount().toString());
    m_scheduleTitle->setText(title);
    m_schedule->setAccount(*a);
}

void QistPage::recordPayment()
{
    const QistAccount *a = selected();
    if (!a || a->state != QistState::Active)
        return;
    QistPaymentDialog dialog(m_ui, *a, window());
    dialog.exec();
}

// ====================================================================== My qist (customer)
MyQistPage::MyQistPage(UiContext &ui, QWidget *parent)
    : Page(ui, parent)
{
    addHeader(QStringLiteral("My qist"),
              QStringLiteral("What you are buying in installments, and when the next payment is due."));

    m_stack = new QStackedWidget();
    m_empty = new EmptyState(QStringLiteral("No qist yet"), QString(), QStringLiteral("Continue shopping"));
    connect(m_empty, &EmptyState::actionClicked, this, [this] { emit m_ui.events->openShop(0, QString()); });
    m_stack->addWidget(m_empty);

    auto *content = new QWidget();
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(16);
    auto *cards = new QGridLayout();
    cards->setHorizontalSpacing(14);
    m_left = new StatCard(QStringLiteral("Left to pay"));
    m_next = new StatCard(QStringLiteral("Next installment"));
    m_overdue = new StatCard(QStringLiteral("Overdue"));
    cards->addWidget(m_left, 0, 0);
    cards->addWidget(m_next, 0, 1);
    cards->addWidget(m_overdue, 0, 2);
    v->addLayout(cards);

    m_model = new QistAccountTableModel(false, this);
    m_table = new QTableView();
    m_table->setModel(m_model);
    setupTable(m_table);
    m_table->setSortingEnabled(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    v->addWidget(m_table, 1);

    m_scheduleTitle = makeLabel(QStringLiteral("Schedule"), "h2");
    v->addWidget(m_scheduleTitle);
    m_schedule = new QistScheduleTableModel(this);
    m_scheduleTable = new QTableView();
    m_scheduleTable->setModel(m_schedule);
    setupScheduleTable(m_scheduleTable);
    v->addWidget(m_scheduleTable, 2);
    v->addWidget(makeLabel(QStringLiteral("Pay the shop by cash, Easypaisa, JazzCash or bank transfer; "
                                          "the shop records each installment here and in your khata."),
                           "small"));
    m_stack->addWidget(content);
    m_layout->addWidget(m_stack, 1);

    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MyQistPage::loadSchedule);
}

const QistAccount *MyQistPage::selected() const
{
    const QModelIndex idx = m_table->currentIndex();
    return idx.isValid() ? m_model->accountAt(idx.row()) : nullptr;
}

void MyQistPage::refresh()
{
    auto accounts = m_ui.app.qist().myAccounts(m_ui.session);
    if (!accounts) {
        emit m_ui.events->toast(accounts.error());
        return;
    }
    if (accounts.value().isEmpty()) {
        auto eligible = m_ui.app.qist().myEligibility(m_ui.session);
        const bool approved = eligible && eligible.value().approved;
        m_empty->setTexts(QStringLiteral("No qist yet"),
                          approved ? QStringLiteral("Items marked \"Qist\" can be bought in monthly installments. "
                                                    "Choose \"Qist (installments)\" at checkout.")
                                   : QStringLiteral("Qist is not open for your account yet. Ask the shop to allow "
                                                    "qist, then buy items marked \"Qist\" in monthly installments."));
        m_stack->setCurrentIndex(0);
        return;
    }
    m_stack->setCurrentIndex(1);

    Money left;
    Money overdue;
    std::optional<InstallmentStatus> next;
    for (const QistAccount &a : accounts.value()) {
        left += a.left();
        overdue += a.overdueAmount();
        const auto mine = a.nextDue();
        if (mine && (!next || mine->installment.dueDate < next->installment.dueDate))
            next = mine;
    }
    m_left->setValue(left.toString(), left.isZero() ? QStringLiteral("All paid up") : QStringLiteral("On all your qists"));
    if (next)
        m_next->setValue(next->remaining().toString(), QStringLiteral("Due %1").arg(dayText(next->installment.dueDate)),
                         next->state == InstallmentState::Overdue ? QStringLiteral("error") : QStringLiteral("small"));
    else
        m_next->setValue(QStringLiteral("-"), QStringLiteral("Nothing due"));
    m_overdue->setValue(overdue.toString(),
                        overdue.isZero() ? QStringLiteral("All on time") : QStringLiteral("Please pay the shop soon"),
                        overdue.isZero() ? QStringLiteral("small") : QStringLiteral("error"));

    const qint64 keepId = selected() ? selected()->id : 0;
    m_model->setAccounts(accounts.value());
    int row = 0;
    for (int r = 0; r < m_model->rowCount(); ++r) {
        if (m_model->accountAt(r)->id == keepId)
            row = r;
    }
    m_table->selectRow(row);
    loadSchedule();
}

void MyQistPage::loadSchedule()
{
    const QistAccount *a = selected();
    if (!a) {
        m_scheduleTitle->setText(QStringLiteral("Schedule"));
        m_schedule->clear();
        return;
    }
    m_scheduleTitle->setText(QStringLiteral("Schedule - order %1 (%2)").arg(a->orderNumber, a->planName));
    m_schedule->setAccount(*a);
}

} // namespace shophub::ui
