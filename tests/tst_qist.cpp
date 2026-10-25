#include "TestSupport.h"

#include "database/SqlUtil.h"
#include "domain/Qist.h"

using namespace shophub;

// Qist (installments), v2 milestone 3: the arithmetic in domain/Qist.h, plans,
// who may buy what, checkout on qist, the down payment before delivery,
// installments, completion, overdue, cancelling, the khata link and rollback.
class TestQist : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<test::Backend> m_b;
    qint64 m_phone = 0;  // Rs 60,000, may be sold on qist (free delivery)
    qint64 m_kettle = 0; // Rs 2,000, cash or khata only
    qint64 m_plan = 0;   // "6 months": 10% markup, at least 20% down

    QistService &qist() { return m_b->ctx->qist(); }
    qint64 count(const QString &sql) { return m_b->scalar(sql).toLongLong(); }

    static QistTerms terms(int months, int markupBp, int minDownBp) { return QistTerms{months, markupBp, minDownBp}; }

    Result<Order> buy(qint64 productId, int qty, const QString &method, qint64 planId = 0,
                      std::optional<Money> down = std::nullopt, const Session *who = nullptr)
    {
        const Session &customer = who ? *who : m_b->customer;
        auto added = m_b->ctx->cart().add(customer, productId, qty);
        if (!added)
            return added;
        CheckoutRequest req = m_b->checkoutRequest();
        req.paymentMethod = method;
        req.qistPlanId = planId;
        req.qistDownPayment = down;
        return m_b->ctx->checkout().placeOrder(customer, req);
    }

    // Allows qist for the test customer and buys the phone on the 6-month plan.
    QistAccount buyPhoneOnQist()
    {
        if (!qist().setApproved(m_b->admin, m_b->customer.userId(), true))
            qWarning() << "approve failed";
        auto order = buy(m_phone, 1, payment::kQist, m_plan);
        if (!order) {
            qWarning() << order.error();
            return QistAccount();
        }
        auto found = qist().forOrder(m_b->admin, order.value().id);
        return found && found.value() ? *found.value() : QistAccount();
    }

    // The running balance on the newest entry must equal the sum of all entries.
    void verifyLedger(qint64 userId)
    {
        const qint64 sum = count(QStringLiteral("SELECT COALESCE(SUM(amount_paisa), 0) FROM khata_entries WHERE user_id = %1").arg(userId));
        const qint64 last = count(QStringLiteral("SELECT COALESCE((SELECT balance_after_paisa FROM khata_entries "
                                                 "WHERE user_id = %1 ORDER BY id DESC LIMIT 1), 0)").arg(userId));
        QCOMPARE(last, sum);
    }

    static QVector<QistInstallment> smallSchedule(const QDate &today)
    {
        // down payment 1,000 forty days ago, then 500 ten days ago, today and in 30 days
        return {{0, today.addDays(-40), Money::fromRupees(1000)},
                {1, today.addDays(-10), Money::fromRupees(500)},
                {2, today, Money::fromRupees(500)},
                {3, today.addDays(30), Money::fromRupees(500)}};
    }

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
        m_phone = m_b->addProduct(QStringLiteral("PH-1"), 60000, 5, QStringLiteral("Phone"));
        m_kettle = m_b->addProduct(QStringLiteral("KET-1"), 2000, 50, QStringLiteral("Kettle"));
        QVERIFY(m_b->ctx->catalog().setQistAllowed(m_b->admin, m_phone, true).isOk());
        auto plan = qist().createPlan(m_b->admin, QistPlanInput{QStringLiteral("6 months"), terms(6, 1000, 2000)});
        QVERIFY2(plan.isOk(), qPrintable(plan.error()));
        m_plan = plan.value();
    }
    void cleanup() { m_b.reset(); }

    // ------------------------------------------------------------------ arithmetic (domain/Qist.h)
    void quoteMatchesTheExample()
    {
        // docs/QIST.md: Rs 60,000 on "6 months: 10% markup, at least 20% down"
        const QDate start(2026, 10, 24);
        auto q = qist::quote(terms(6, 1000, 2000), Money::fromRupees(60000), std::nullopt, start);
        QVERIFY2(q.isOk(), qPrintable(q.error()));
        const QistQuote &v = q.value();
        QCOMPARE(v.markup, Money::fromRupees(6000));
        QCOMPARE(v.qistPrice, Money::fromRupees(66000));
        QCOMPARE(v.minimumDown, Money::fromRupees(13200));
        QCOMPARE(v.downPayment, Money::fromRupees(13200));
        QCOMPARE(v.monthly, Money::fromRupees(8800));
        QCOMPARE(v.lastMonthly, Money::fromRupees(8800));
        QCOMPARE(v.schedule.size(), 7);
        QCOMPARE(v.schedule.first().number, 0);
        QCOMPARE(v.schedule.first().dueDate, start);
        QCOMPARE(v.schedule.at(1).dueDate, QDate(2026, 11, 24));
        QCOMPARE(v.schedule.last().number, 6);
        QCOMPARE(v.schedule.last().dueDate, QDate(2027, 4, 24));
        Money sum;
        for (const QistInstallment &line : v.schedule)
            sum += line.amount;
        QCOMPARE(sum, v.qistPrice);
    }

    void oddAmountsGoOnTheLastInstallment()
    {
        // Rs 21,999 on the same plan: markup Rs 2,199.90, qist price Rs 24,198.90
        auto q = qist::quote(terms(6, 1000, 2000), Money::fromRupees(21999), std::nullopt, QDate(2026, 10, 24));
        QVERIFY(q.isOk());
        const QistQuote &v = q.value();
        QCOMPARE(v.markup, Money::fromPaisa(219990));
        QCOMPARE(v.qistPrice, Money::fromPaisa(2419890));
        QCOMPARE(v.minimumDown, Money::fromRupees(4840)); // 20% = 4,839.78, rounded up to a whole rupee
        QCOMPARE(v.monthly, Money::fromRupees(3226));      // whole rupees
        QCOMPARE(v.lastMonthly, Money::fromPaisa(322890)); // Rs 3,228.90
        Money sum;
        for (const QistInstallment &line : v.schedule)
            sum += line.amount;
        QCOMPARE(sum, v.qistPrice);

        QCOMPARE(qist::minimumDown(Money::fromPaisa(2419890), 0), Money());
        QCOMPARE(qist::minimumDown(Money::fromPaisa(50), 9000), Money::fromPaisa(50)); // never above the price
    }

    void dueDatesFollowTheCalendar()
    {
        auto q = qist::quote(terms(3, 0, 0), Money::fromRupees(30000), std::nullopt, QDate(2026, 1, 31));
        QVERIFY(q.isOk());
        QCOMPARE(q.value().schedule.size(), 3); // no down payment line when the down payment is zero
        QCOMPARE(q.value().schedule.at(0).dueDate, QDate(2026, 2, 28));
        QCOMPARE(q.value().schedule.at(1).dueDate, QDate(2026, 3, 31));
        QCOMPARE(q.value().schedule.at(2).dueDate, QDate(2026, 4, 30));
        auto leap = qist::quote(terms(2, 0, 0), Money::fromRupees(30000), std::nullopt, QDate(2028, 1, 31));
        QVERIFY(leap.isOk());
        QCOMPARE(leap.value().schedule.at(0).dueDate, QDate(2028, 2, 29));
    }

    void quoteRules()
    {
        const QDate start(2026, 10, 24);
        const QistTerms plan = terms(6, 1000, 2000);
        const Money cash = Money::fromRupees(60000);
        QVERIFY(!qist::quote(plan, cash, Money::fromPaisa(1319999), start).isOk()); // below the minimum
        QVERIFY(qist::quote(plan, cash, Money::fromRupees(13200), start).isOk());   // exactly the minimum
        QVERIFY(!qist::quote(plan, cash, Money::fromRupees(66000), start).isOk());  // everything now: pay cash
        QVERIFY(!qist::quote(plan, cash, Money() - Money::fromRupees(5), start).isOk());
        QVERIFY(!qist::quote(plan, Money(), std::nullopt, start).isOk());

        auto more = qist::quote(plan, cash, Money::fromRupees(20000), start); // a bigger down payment
        QVERIFY(more.isOk());
        QCOMPARE(more.value().monthly, Money::fromRupees(7666));
        QCOMPARE(more.value().lastMonthly, Money::fromRupees(7670));

        // Each monthly installment must be at least Rs 100.
        QVERIFY(!qist::quote(terms(12, 0, 0), Money::fromRupees(1000), std::nullopt, start).isOk());
        QVERIFY(qist::quote(terms(6, 0, 0), Money::fromRupees(600), std::nullopt, start).isOk());
        QVERIFY(!qist::quote(terms(6, 0, 0), Money::fromPaisa(59999), std::nullopt, start).isOk());

        // Plan limits
        QVERIFY(qist::validateTerms(terms(2, 0, 0)).isEmpty());
        QVERIFY(qist::validateTerms(terms(24, 5000, 9000)).isEmpty());
        QVERIFY(!qist::validateTerms(terms(1, 0, 0)).isEmpty());
        QVERIFY(!qist::validateTerms(terms(25, 0, 0)).isEmpty());
        QVERIFY(!qist::validateTerms(terms(6, 5001, 0)).isEmpty());
        QVERIFY(!qist::validateTerms(terms(6, -1, 0)).isEmpty());
        QVERIFY(!qist::validateTerms(terms(6, 0, 9001)).isEmpty());
        QVERIFY(!qist::quote(terms(1, 0, 0), cash, std::nullopt, start).isOk());
    }

    void paymentsGoToTheOldestInstallment()
    {
        const QDate today(2026, 10, 24);
        const QVector<QistInstallment> schedule = smallSchedule(today);

        auto lines = qist::applyPayments(schedule, Money::fromRupees(1200), today);
        QCOMPARE(lines.size(), 4);
        QCOMPARE(lines[0].state, InstallmentState::Paid);
        QCOMPARE(lines[1].paid, Money::fromRupees(200));
        QCOMPARE(lines[1].remaining(), Money::fromRupees(300));
        QCOMPARE(lines[1].state, InstallmentState::Overdue);
        QCOMPARE(lines[2].state, InstallmentState::DueToday);
        QCOMPARE(lines[3].state, InstallmentState::Upcoming);
        QCOMPARE(qist::overdueAmount(lines), Money::fromRupees(300));
        QCOMPARE(qist::daysOverdue(lines, today), 10);
        QCOMPARE(qist::nextUnpaid(lines)->installment.number, 1);

        lines = qist::applyPayments(schedule, Money(), today);
        QCOMPARE(qist::overdueAmount(lines), Money::fromRupees(1500));
        QCOMPARE(qist::daysOverdue(lines, today), 40);

        lines = qist::applyPayments(schedule, Money::fromRupees(2500), today);
        for (const InstallmentStatus &s : lines)
            QCOMPARE(s.state, InstallmentState::Paid);
        QVERIFY(!qist::nextUnpaid(lines).has_value());
        QCOMPARE(qist::daysOverdue(lines, today), 0);
    }

    void paymentNotesSayWhatWasPaid()
    {
        const QVector<QistInstallment> schedule = smallSchedule(QDate(2026, 10, 24));
        QCOMPARE(qist::describePayment(schedule, Money(), Money::fromRupees(1000)), QStringLiteral("Down payment"));
        QCOMPARE(qist::describePayment(schedule, Money::fromRupees(1000), Money::fromRupees(500)),
                 QStringLiteral("Installment 1 of 3"));
        QCOMPARE(qist::describePayment(schedule, Money::fromRupees(1000), Money::fromRupees(200)),
                 QStringLiteral("Part of installment 1 of 3"));
        QCOMPARE(qist::describePayment(schedule, Money::fromRupees(1200), Money::fromRupees(300)),
                 QStringLiteral("Installment 1 of 3"));
        QCOMPARE(qist::describePayment(schedule, Money(), Money::fromRupees(1500)),
                 QStringLiteral("Down payment to installment 1 of 3"));
        QCOMPARE(qist::describePayment(schedule, Money(), Money::fromRupees(1700)),
                 QStringLiteral("Down payment to installment 2 of 3 (part)"));
    }

    // ------------------------------------------------------------------ plans
    void plansNeedAnAdminAndValidTerms()
    {
        QVERIFY(!qist().createPlan(m_b->customer, QistPlanInput{QStringLiteral("Mine"), terms(6, 0, 0)}).isOk());
        QVERIFY(!qist().createPlan(m_b->admin, QistPlanInput{QStringLiteral("  "), terms(6, 0, 0)}).isOk());
        QVERIFY(!qist().createPlan(m_b->admin, QistPlanInput{QStringLiteral("One month"), terms(1, 0, 0)}).isOk());
        QVERIFY(!qist().createPlan(m_b->admin, QistPlanInput{QStringLiteral("Greedy"), terms(6, 6000, 0)}).isOk());
        auto duplicate = qist().createPlan(m_b->admin, QistPlanInput{QStringLiteral("6 MONTHS"), terms(6, 0, 0)});
        QVERIFY(!duplicate.isOk());
        QVERIFY(duplicate.error().contains(QStringLiteral("already a plan")));

        auto twelve = qist().createPlan(m_b->admin, QistPlanInput{QStringLiteral("12 months"), terms(12, 2000, 2000)});
        QVERIFY(twelve.isOk());
        QVERIFY(qist().updatePlan(m_b->admin, twelve.value(), QistPlanInput{QStringLiteral("12 months easy"), terms(12, 1500, 1000)}).isOk());
        QVERIFY(!qist().updatePlan(m_b->admin, twelve.value(), QistPlanInput{QStringLiteral("6 months"), terms(12, 0, 0)}).isOk());
        QVERIFY(!qist().updatePlan(m_b->customer, twelve.value(), QistPlanInput{QStringLiteral("x"), terms(12, 0, 0)}).isOk());

        auto plans = qist().plans(m_b->customer);
        QVERIFY(plans.isOk());
        QCOMPARE(plans.value().size(), 2);
        QCOMPARE(plans.value().first().name, QStringLiteral("6 months")); // sorted by months
        QCOMPARE(plans.value().last().terms.markupBp, 1500);

        // A switched-off plan is not offered, but admins still see it.
        QVERIFY(qist().setPlanActive(m_b->admin, twelve.value(), false).isOk());
        QCOMPARE(qist().plans(m_b->customer).value().size(), 1);
        QCOMPARE(qist().plans(m_b->customer, true).value().size(), 1); // customers never see switched-off plans
        QCOMPARE(qist().plans(m_b->admin, true).value().size(), 2);
        QVERIFY(!qist().quote(m_b->customer, twelve.value(), Money::fromRupees(10000)).isOk());
        QVERIFY(!qist().setPlanActive(m_b->customer, twelve.value(), true).isOk());
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM audit_logs WHERE action LIKE 'qist_plan_%'")), 4);

        // The service quote is the arithmetic above, for the plan's terms.
        auto quote = qist().quote(m_b->customer, m_plan, Money::fromRupees(60000));
        QVERIFY(quote.isOk());
        QCOMPARE(quote.value().qistPrice, Money::fromRupees(66000));
        QCOMPARE(quote.value().schedule.first().dueDate, QistService::today());
    }

    // ------------------------------------------------------------------ who may buy what
    void approvalRules()
    {
        const qint64 me = m_b->customer.userId();
        QVERIFY(!qist().setApproved(m_b->customer, me, true).isOk());
        QVERIFY(!qist().setApproved(m_b->admin, m_b->admin.userId(), true).isOk()); // not a customer
        QVERIFY(!qist().setApproved(m_b->admin, 999999, true).isOk());
        QVERIFY(!qist().myEligibility(m_b->customer).value().approved);

        QVERIFY(qist().setApproved(m_b->admin, me, true).isOk());
        QVERIFY(qist().myEligibility(m_b->customer).value().approved);
        QCOMPARE(qist().myEligibility(m_b->customer).value().plans.size(), 1);
        // The approval lives on the customer's khata row; no credit is given with it.
        QCOMPARE(m_b->ctx->khata().myAccount(m_b->customer).value().creditLimit, Money());
        QVERIFY(m_b->ctx->khata().myAccount(m_b->customer).value().qistApproved);

        // Setting a credit limit keeps the approval, and the other way round.
        QVERIFY(m_b->ctx->khata().setLimit(m_b->admin, me, Money::fromRupees(5000)).isOk());
        QVERIFY(m_b->ctx->khata().myAccount(m_b->customer).value().qistApproved);
        QVERIFY(qist().setApproved(m_b->admin, me, false).isOk());
        QCOMPARE(m_b->ctx->khata().myAccount(m_b->customer).value().creditLimit, Money::fromRupees(5000));
        QVERIFY(!m_b->ctx->khata().myAccount(m_b->customer).value().qistApproved);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_accounts")), 1);
    }

    void checkoutChecksEverything()
    {
        const int stockBefore = m_b->stockOf(m_phone);

        auto r = buy(m_phone, 1, payment::kQist, m_plan);
        QVERIFY(!r.isOk());
        QVERIFY(r.error().contains(QStringLiteral("Qist is not open")));

        QVERIFY(qist().setApproved(m_b->admin, m_b->customer.userId(), true).isOk());
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_kettle, 1).isOk()); // the cart now also has a kettle
        CheckoutRequest req = m_b->checkoutRequest();
        req.paymentMethod = payment::kQist;
        req.qistPlanId = m_plan;
        r = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(!r.isOk());
        QVERIFY2(r.error().contains(QStringLiteral("cannot be bought on qist: Kettle")), qPrintable(r.error()));
        QVERIFY(m_b->ctx->cart().remove(m_b->customer, m_kettle).isOk());

        req = m_b->checkoutRequest();
        req.paymentMethod = payment::kQist; // no plan chosen
        r = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(!r.isOk());
        QVERIFY(r.error().contains(QStringLiteral("choose a qist plan")));

        req.qistPlanId = m_plan;
        req.qistDownPayment = Money::fromRupees(100); // below the minimum
        r = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(!r.isOk());
        QVERIFY(r.error().contains(QStringLiteral("at least Rs 13,200")));

        QVERIFY(qist().setPlanActive(m_b->admin, m_plan, false).isOk());
        req.qistDownPayment.reset();
        QVERIFY(!m_b->ctx->checkout().placeOrder(m_b->customer, req).isOk());

        // Nothing was saved along the way.
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM qist_accounts")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries")), 0);
        QCOMPARE(m_b->stockOf(m_phone), stockBefore);
        QCOMPARE(m_b->ctx->cart().itemCount(m_b->customer).value(), 1);
    }

    // ------------------------------------------------------------------ buying on qist
    void qistOrderCreatesTheScheduleAndAKhataEntry()
    {
        QVERIFY(m_b->ctx->khata().setLimit(m_b->admin, m_b->customer.userId(), Money::fromRupees(10000)).isOk());
        const QistAccount q = buyPhoneOnQist();
        QVERIFY(q.id > 0);

        auto order = m_b->ctx->orders().details(m_b->customer, q.orderId);
        QVERIFY(order.isOk());
        QCOMPARE(order.value().paymentMethod, payment::kQist);
        QCOMPARE(order.value().paymentStatus, payment::kOnAccount);
        QCOMPARE(order.value().total, Money::fromRupees(60000)); // the order keeps the cash price
        QCOMPARE(m_b->stockOf(m_phone), 4);
        QCOMPARE(m_b->ctx->cart().itemCount(m_b->customer).value(), 0);

        QCOMPARE(q.planName, QStringLiteral("6 months"));
        QCOMPARE(q.qistPrice, Money::fromRupees(66000));
        QCOMPARE(q.downPayment, Money::fromRupees(13200));
        QCOMPARE(q.paid, Money());
        QCOMPARE(q.state, QistState::Active);
        QCOMPARE(q.schedule.size(), 7);
        QCOMPARE(q.schedule.first().dueDate, QistService::today());
        QCOMPARE(count(QStringLiteral("SELECT SUM(amount_paisa) FROM qist_installments")), qint64(6600000));
        QCOMPARE(q.installments.first().state, InstallmentState::DueToday);
        QVERIFY(!q.isOverdue());

        // The qist price is on the khata ledger, linked to the qist...
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE kind = 'charge' AND qist_id = %1 "
                                      "AND amount_paisa = 6600000").arg(q.id)), 1);
        const KhataAccount k = m_b->ctx->khata().myAccount(m_b->customer).value();
        QCOMPARE(k.balance, Money::fromRupees(66000));
        QCOMPARE(k.qistBalance, Money::fromRupees(66000));
        // ...but it does not use the khata credit limit.
        QCOMPARE(k.creditUsed(), Money());
        QCOMPARE(k.available(), Money::fromRupees(10000));
        QVERIFY(!k.overLimit());
        verifyLedger(m_b->customer.userId());

        auto stats = m_b->ctx->dashboard().stats(m_b->admin).value();
        QCOMPARE(stats.qistOutstanding, Money::fromRupees(66000));
        QCOMPARE(stats.qistOverdue, 0);
        QCOMPARE(stats.khataOutstanding, Money());

        // A double-click on "Place order" never makes a second qist.
        QVERIFY(m_b->ctx->cart().add(m_b->customer, m_phone, 1).isOk());
        CheckoutRequest req = m_b->checkoutRequest();
        req.paymentMethod = payment::kQist;
        req.qistPlanId = m_plan;
        auto first = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(first.isOk());
        auto again = m_b->ctx->checkout().placeOrder(m_b->customer, req);
        QVERIFY(again.isOk());
        QCOMPARE(again.value().id, first.value().id);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM qist_accounts")), 2);
    }

    void aBiggerDownPaymentIsKept()
    {
        QVERIFY(qist().setApproved(m_b->admin, m_b->customer.userId(), true).isOk());
        auto order = buy(m_phone, 1, payment::kQist, m_plan, Money::fromRupees(20000));
        QVERIFY2(order.isOk(), qPrintable(order.error()));
        const QistAccount q = *qist().forOrder(m_b->customer, order.value().id).value();
        QCOMPARE(q.downPayment, Money::fromRupees(20000));
        QCOMPARE(q.schedule.at(1).amount, Money::fromRupees(7666));
        QCOMPARE(q.schedule.last().amount, Money::fromRupees(7670));
    }

    void downPaymentComesBeforeDelivery()
    {
        const QistAccount q = buyPhoneOnQist();
        for (OrderStatus s : {OrderStatus::Confirmed, OrderStatus::Processing, OrderStatus::Shipped})
            QVERIFY(m_b->ctx->orders().advance(m_b->admin, q.orderId, s).isOk());
        const Status refused = m_b->ctx->orders().advance(m_b->admin, q.orderId, OrderStatus::Delivered);
        QVERIFY(!refused.isOk());
        QVERIFY2(refused.error().contains(QStringLiteral("down payment (Rs 13,200.00)")), qPrintable(refused.error()));

        auto paid = qist().recordPayment(m_b->admin, q.id, Money::fromRupees(13200), QStringLiteral("Cash"));
        QVERIFY2(paid.isOk(), qPrintable(paid.error()));
        QVERIFY(paid.value().downPaymentPaid());
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT note FROM khata_entries WHERE kind = 'payment'")).toString(),
                 QStringLiteral("Down payment - Cash"));
        QVERIFY(m_b->ctx->orders().advance(m_b->admin, q.orderId, OrderStatus::Delivered).isOk());
        // Delivered, but still being paid off.
        QCOMPARE(m_b->ctx->orders().details(m_b->admin, q.orderId).value().paymentStatus, payment::kOnAccount);
    }

    void installmentsUntilPaidOff()
    {
        const QistAccount q = buyPhoneOnQist();
        QVERIFY(!qist().recordPayment(m_b->customer, q.id, Money::fromRupees(13200), QString()).isOk());
        QVERIFY(!qist().recordPayment(m_b->admin, q.id, Money(), QString()).isOk());
        auto tooMuch = qist().recordPayment(m_b->admin, q.id, Money::fromRupees(66001), QString());
        QVERIFY(!tooMuch.isOk());
        QVERIFY(tooMuch.error().contains(QStringLiteral("Only Rs 66,000.00 is left")));

        QVERIFY(qist().recordPayment(m_b->admin, q.id, Money::fromRupees(13200), QStringLiteral("Cash")).isOk());
        QVERIFY(qist().recordPayment(m_b->admin, q.id, Money::fromRupees(5000), QStringLiteral("Easypaisa")).isOk());
        auto partly = qist().account(m_b->admin, q.id).value();
        QCOMPARE(partly.paid, Money::fromRupees(18200));
        QCOMPARE(partly.left(), Money::fromRupees(47800));
        QCOMPARE(partly.nextDue()->installment.number, 1);
        QCOMPARE(partly.nextDue()->remaining(), Money::fromRupees(3800));
        QCOMPARE(m_b->scalar(QStringLiteral("SELECT note FROM khata_entries WHERE kind = 'payment' ORDER BY id DESC LIMIT 1"))
                     .toString(),
                 QStringLiteral("Part of installment 1 of 6 - Easypaisa"));

        // The last rupee completes the qist and marks the order paid.
        auto done = qist().recordPayment(m_b->admin, q.id, partly.left(), QStringLiteral("Bank transfer"));
        QVERIFY2(done.isOk(), qPrintable(done.error()));
        QCOMPARE(done.value().state, QistState::Completed);
        QCOMPARE(done.value().statusText(), QStringLiteral("Completed"));
        QCOMPARE(done.value().left(), Money());
        QCOMPARE(m_b->ctx->orders().details(m_b->admin, q.orderId).value().paymentStatus, payment::kPaid);
        auto again = qist().recordPayment(m_b->admin, q.id, Money::fromRupees(1), QString());
        QVERIFY(!again.isOk());
        QVERIFY(again.error().contains(QStringLiteral("already paid in full")));

        const KhataAccount k = m_b->ctx->khata().myAccount(m_b->customer).value();
        QCOMPARE(k.qistBalance, Money());
        QCOMPARE(k.balance, Money());
        verifyLedger(m_b->customer.userId());
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM audit_logs WHERE action = 'qist_payment'")), 3);
        QCOMPARE(m_b->ctx->dashboard().stats(m_b->admin).value().qistOutstanding, Money());
    }

    void overdueInstallmentsBlockANewQist()
    {
        QVERIFY(qist().setApproved(m_b->admin, m_b->customer.userId(), true).isOk());
        qint64 orderId = 0;
        {
            // Bought 70 days ago: the down payment and two installments are now late.
            sql::ScopedFixedClock clock(QDateTime::currentDateTimeUtc().addDays(-70));
            auto order = buy(m_phone, 1, payment::kQist, m_plan);
            QVERIFY2(order.isOk(), qPrintable(order.error()));
            orderId = order.value().id;
        }
        const QistAccount q = *qist().forOrder(m_b->admin, orderId).value();
        QVERIFY(q.isOverdue());
        QCOMPARE(q.statusText(), QStringLiteral("Overdue"));
        const int late = qist::daysOverdue(q.installments, QistService::today());
        QVERIFY2(late >= 69 && late <= 71, qPrintable(QString::number(late)));
        QCOMPARE(q.overdueAmount(), Money::fromRupees(13200 + 8800 + 8800));

        QCOMPARE(qist().myEligibility(m_b->customer).value().overdue, q.overdueAmount());
        auto blocked = buy(m_phone, 1, payment::kQist, m_plan);
        QVERIFY(!blocked.isOk());
        QVERIFY(blocked.error().contains(QStringLiteral("overdue")));
        // Cash on Delivery is still fine.
        QVERIFY(m_b->ctx->checkout().placeOrder(m_b->customer, m_b->checkoutRequest()).isOk());

        QistQuery overdueOnly;
        overdueOnly.show = QistQuery::Show::Overdue;
        QCOMPARE(qist().accounts(m_b->admin, overdueOnly).value().size(), 1);
        QCOMPARE(m_b->ctx->dashboard().stats(m_b->admin).value().qistOverdue, 1);

        // Paying what is late lifts the block.
        QVERIFY(qist().recordPayment(m_b->admin, q.id, q.overdueAmount(), QStringLiteral("Cash")).isOk());
        QCOMPARE(qist().myEligibility(m_b->customer).value().overdue, Money());
        QCOMPARE(qist().accounts(m_b->admin, overdueOnly).value().size(), 0);
        QVERIFY(buy(m_phone, 1, payment::kQist, m_plan).isOk());
    }

    // ------------------------------------------------------------------ cancelling
    void cancellingTakesTheQistOffTheKhata()
    {
        const QistAccount q = buyPhoneOnQist();
        QVERIFY(qist().recordPayment(m_b->admin, q.id, Money::fromRupees(13200), QStringLiteral("Cash")).isOk());
        QVERIFY(m_b->ctx->orders().cancel(m_b->customer, q.orderId, QStringLiteral("Changed my mind")).isOk());

        const QistAccount after = qist().account(m_b->customer, q.id).value();
        QCOMPARE(after.state, QistState::Cancelled);
        QCOMPARE(after.statusText(), QStringLiteral("Cancelled"));
        QCOMPARE(after.left(), Money());
        QVERIFY(!after.nextDue().has_value());
        QCOMPARE(m_b->stockOf(m_phone), 5);
        // charge +66,000, payment -13,200, reversal -66,000, money returned +13,200
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries WHERE qist_id = %1").arg(q.id)), 4);
        QCOMPARE(count(QStringLiteral("SELECT SUM(amount_paisa) FROM khata_entries WHERE qist_id = %1").arg(q.id)), 0);
        QCOMPARE(count(QStringLiteral("SELECT amount_paisa FROM khata_entries WHERE kind = 'adjustment'")), 1320000);
        QCOMPARE(m_b->ctx->khata().myAccount(m_b->customer).value().balance, Money());
        verifyLedger(m_b->customer.userId());
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM qist_installments WHERE qist_id = %1").arg(q.id)), 7);

        auto refused = qist().recordPayment(m_b->admin, q.id, Money::fromRupees(100), QString());
        QVERIFY(!refused.isOk());
        QVERIFY(refused.error().contains(QStringLiteral("cancelled")));
        QVERIFY(!m_b->ctx->orders().cancel(m_b->admin, q.orderId, QString()).isOk()); // only once
    }

    void cancellingAPaidOffQistReturnsEverything()
    {
        const QistAccount q = buyPhoneOnQist();
        QVERIFY(m_b->ctx->orders().advance(m_b->admin, q.orderId, OrderStatus::Confirmed).isOk());
        QVERIFY(qist().recordPayment(m_b->admin, q.id, Money::fromRupees(66000), QStringLiteral("Cash")).isOk());
        QCOMPARE(qist().account(m_b->admin, q.id).value().state, QistState::Completed);
        QVERIFY(m_b->ctx->orders().cancel(m_b->admin, q.orderId, QStringLiteral("Out of stock at the supplier")).isOk());
        QCOMPARE(qist().account(m_b->admin, q.id).value().state, QistState::Cancelled);
        QCOMPARE(count(QStringLiteral("SELECT SUM(amount_paisa) FROM khata_entries WHERE qist_id = %1").arg(q.id)), 0);
        verifyLedger(m_b->customer.userId());
    }

    // ------------------------------------------------------------------ khata and qist together
    void khataAndQistStaySeparate()
    {
        const qint64 me = m_b->customer.userId();
        QVERIFY(m_b->ctx->khata().setLimit(m_b->admin, me, Money::fromRupees(3000)).isOk());
        const QistAccount q = buyPhoneOnQist();
        QVERIFY(q.id > 0);

        // A khata payment cannot pay the qist.
        auto wrongPlace = m_b->ctx->khata().recordPayment(m_b->admin, me, Money::fromRupees(1000), QStringLiteral("Cash"));
        QVERIFY(!wrongPlace.isOk());
        QVERIFY(wrongPlace.error().contains(QStringLiteral("Qist page")));
        QVERIFY(!m_b->ctx->khata().adjust(m_b->admin, me, Money() - Money::fromRupees(100), QStringLiteral("Discount")).isOk());

        // The khata limit is still free for a khata order.
        auto onKhata = buy(m_kettle, 1, payment::kKhata);
        QVERIFY2(onKhata.isOk(), qPrintable(onKhata.error()));
        KhataAccount k = m_b->ctx->khata().myAccount(m_b->customer).value();
        QCOMPARE(k.creditUsed(), Money::fromRupees(2250));
        QCOMPARE(k.balance, Money::fromRupees(66000 + 2250));
        QCOMPARE(k.available(), Money::fromRupees(750));

        // A khata payment pays only the khata part.
        QVERIFY(!m_b->ctx->khata().recordPayment(m_b->admin, me, Money::fromRupees(2251), QStringLiteral("Cash")).isOk());
        QVERIFY(m_b->ctx->khata().recordPayment(m_b->admin, me, Money::fromRupees(2250), QStringLiteral("Cash")).isOk());
        k = m_b->ctx->khata().myAccount(m_b->customer).value();
        QCOMPARE(k.creditUsed(), Money());
        QCOMPARE(k.qistBalance, Money::fromRupees(66000));
        QCOMPARE(qist().account(m_b->admin, q.id).value().paid, Money()); // untouched

        // Khata figures on the dashboard leave the qist out.
        auto stats = m_b->ctx->dashboard().stats(m_b->admin).value();
        QCOMPARE(stats.khataOutstanding, Money());
        QCOMPARE(stats.khataDebtors, 0);
        QCOMPARE(stats.qistOutstanding, Money::fromRupees(66000));
        verifyLedger(me);
    }

    void customersSeeOnlyTheirOwnQists()
    {
        const QistAccount q = buyPhoneOnQist();
        auto other = m_b->ctx->auth().registerCustomer(QStringLiteral("Other Person"), QStringLiteral("other@test.pk"),
                                                       QStringLiteral("OtherPass1"));
        QVERIFY(other.isOk());
        auto peek = qist().account(other.value(), q.id);
        QVERIFY(!peek.isOk());
        QCOMPARE(peek.error(), QStringLiteral("Qist not found."));
        QVERIFY(!qist().forOrder(other.value(), q.orderId).value().has_value());
        QVERIFY(qist().myAccounts(other.value()).value().isEmpty());
        QCOMPARE(qist().myAccounts(m_b->customer).value().size(), 1);
        QVERIFY(!qist().accounts(m_b->customer, QistQuery()).isOk()); // the admin list is admin only

        QistQuery byName;
        byName.search = QStringLiteral("Test Cust");
        QCOMPARE(qist().accounts(m_b->admin, byName).value().size(), 1);
        byName.search = QStringLiteral("nobody");
        QCOMPARE(qist().accounts(m_b->admin, byName).value().size(), 0);
    }

    // ------------------------------------------------------------------ products
    void productsAreMarkedForQist()
    {
        QVERIFY(m_b->ctx->catalog().product(m_b->customer, m_phone).value().qistAllowed);
        QVERIFY(!m_b->ctx->catalog().product(m_b->customer, m_kettle).value().qistAllowed);
        QVERIFY(!m_b->ctx->catalog().setQistAllowed(m_b->customer, m_kettle, true).isOk());
        QVERIFY(!m_b->ctx->catalog().setQistAllowed(m_b->admin, 999999, true).isOk());

        // The flag is part of the product form too.
        ProductInput in;
        in.sku = QStringLiteral("TV-1");
        in.name = QStringLiteral("Television");
        in.categoryId = m_b->categoryId;
        in.price = Money::fromRupees(80000);
        in.initialStock = 2;
        in.qistAllowed = true;
        auto tv = m_b->ctx->catalog().createProduct(m_b->admin, in);
        QVERIFY(tv.isOk());
        QVERIFY(m_b->ctx->catalog().product(m_b->admin, tv.value()).value().qistAllowed);
        in.qistAllowed = false;
        QVERIFY(m_b->ctx->catalog().updateProduct(m_b->admin, tv.value(), in).isOk());
        QVERIFY(!m_b->ctx->catalog().product(m_b->admin, tv.value()).value().qistAllowed);
        QVERIFY(m_b->ctx->cart().add(m_b->customer, tv.value(), 1).isOk());
        QVERIFY(m_b->ctx->cart().items(m_b->customer).value().first().qistAllowed == false);
    }

    // ------------------------------------------------------------------ all or nothing
    void failedQistCheckoutLeavesNoTrace()
    {
        QVERIFY(qist().setApproved(m_b->admin, m_b->customer.userId(), true).isOk());
        m_b->ctx->checkout().setFaultInjectorForTesting([](const QString &stage) {
            return stage == QLatin1String("after_qist_created") ? Status::fail(QStringLiteral("injected"))
                                                                 : Status::ok();
        });
        auto r = buy(m_phone, 1, payment::kQist, m_plan);
        QVERIFY(!r.isOk());
        m_b->ctx->checkout().setFaultInjectorForTesting({});

        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM orders")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM qist_accounts")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM qist_installments")), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM khata_entries")), 0);
        QCOMPARE(m_b->stockOf(m_phone), 5);
        QCOMPARE(m_b->ctx->cart().itemCount(m_b->customer).value(), 1);
    }
};

QTEST_GUILESS_MAIN(TestQist)
#include "tst_qist.moc"
