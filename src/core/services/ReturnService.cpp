#include "services/ReturnService.h"

#include "database/SqlUtil.h"
#include "database/Transaction.h"
#include "services/Access.h"
#include "services/InvoiceService.h"
#include "services/KhataService.h"

#include <QLocale>
#include <QSet>
#include <QtDebug>

namespace shophub {

namespace {

const QString kWindowKey = QStringLiteral("return_window_days");
const QString kChangedMindKey = QStringLiteral("return_allow_changed_mind");

QString dayText(const QDate &date)
{
    return QLocale(QLocale::English).toString(date, QStringLiteral("d MMM yyyy"));
}

QString cleanNote(const QString &text)
{
    return text.simplified();
}

} // namespace

ReturnService::ReturnService(Repositories &repos)
    : m_repos(repos)
{
}

// ---------------------------------------------------------------- settings

Result<ReturnSettings> ReturnService::readSettings() const
{
    ReturnSettings s;
    auto window = m_repos.settings.get(kWindowKey, QString::number(returns::kDefaultWindowDays));
    if (!window)
        return window.status();
    bool ok = false;
    const int days = window.value().toInt(&ok);
    s.windowDays = ok && days >= 0 && days <= returns::kMaxWindowDays ? days : returns::kDefaultWindowDays;
    auto changedMind = m_repos.settings.get(kChangedMindKey, QStringLiteral("1"));
    if (!changedMind)
        return changedMind.status();
    s.allowChangedMind = changedMind.value() != QLatin1String("0");
    return s;
}

Result<ReturnSettings> ReturnService::settings(const Session &session) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return readSettings();
}

Result<ReturnSettings> ReturnService::policy(const Session &session) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    return readSettings();
}

Status ReturnService::saveSettings(const Session &session, const ReturnSettings &settings)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (settings.windowDays < 0 || settings.windowDays > returns::kMaxWindowDays)
        return Status::fail(QStringLiteral("The return window must be between 0 and %1 days.").arg(returns::kMaxWindowDays));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    if (auto st = m_repos.settings.set(kWindowKey, QString::number(settings.windowDays)); !st)
        return st;
    if (auto st = m_repos.settings.set(kChangedMindKey, settings.allowChangedMind ? QStringLiteral("1") : QStringLiteral("0"));
        !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("return_settings_changed"), QStringLiteral("settings"),
                                    0,
                                    QStringLiteral("window %1 days, changed mind %2")
                                        .arg(settings.windowDays)
                                        .arg(settings.allowChangedMind ? QStringLiteral("allowed") : QStringLiteral("not allowed")));
        !st)
        return st;
    return tx.commit();
}

// ---------------------------------------------------------------- loading

Result<Order> ReturnService::loadOrder(const Session &session, qint64 orderId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.orders.findById(orderId, true);
    if (!found)
        return found.status();
    // Same message for "missing" and "someone else's", so order ids can't be probed.
    if (!found.value() || (!session.isAdmin() && found.value()->userId != session.userId()))
        return Result<Order>::fail(QStringLiteral("Order not found."));
    return *found.value();
}

Result<ReturnRequest> ReturnService::loadReturn(const Session &session, qint64 returnId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.returns.findById(returnId);
    if (!found)
        return found.status();
    if (!found.value() || (!session.isAdmin() && found.value()->userId != session.userId()))
        return Result<ReturnRequest>::fail(QStringLiteral("Return not found."));
    return *found.value();
}

// ---------------------------------------------------------------- asking

Result<ReturnEligibility> ReturnService::eligibility(const Session &session, qint64 orderId) const
{
    auto order = loadOrder(session, orderId);
    if (!order)
        return order.status();
    const Order &o = order.value();
    auto settings = readSettings();
    if (!settings)
        return settings.status();

    ReturnEligibility e;
    e.changedMindAllowed = settings.value().allowChangedMind || session.isAdmin();

    if (o.status != OrderStatus::Delivered) {
        e.why = o.status == OrderStatus::Cancelled
                    ? QStringLiteral("This order was cancelled, so there is nothing to return.")
                    : QStringLiteral("Items can be returned once the order has been delivered.");
        return e;
    }

    auto delivered = m_repos.orders.deliveredAt(orderId);
    if (!delivered)
        return delivered.status();
    e.deliveredAt = delivered.value();
    // Orders delivered before ShopHub recorded the time have no date: treat them as delivered when placed.
    const QDateTime from = e.deliveredAt.isValid() ? e.deliveredAt : o.createdAt;
    e.lastDay = returns::lastDay(from, settings.value().windowDays);
    e.late = sql::todayLocal() > e.lastDay;

    auto returned = m_repos.returns.returnedUnits(orderId);
    if (!returned)
        return returned.status();
    int available = 0;
    for (const OrderItem &item : o.items) {
        ReturnableItem ri;
        ri.line.orderItemId = item.id;
        ri.line.productId = item.productId;
        ri.line.sku = item.sku;
        ri.line.productName = item.productName;
        ri.line.unitPrice = item.unitPrice;
        ri.line.quantity = item.quantity;
        ri.alreadyReturned = returned.value().value(item.id);
        available += ri.available();
        e.items.push_back(ri);
    }

    auto existing = m_repos.returns.forOrder(orderId);
    if (!existing)
        return existing.status();
    for (const ReturnRequest &r : existing.value()) {
        if (returns::isOpen(r.status)) {
            e.why = QStringLiteral("Return %1 for this order is still open (%2).")
                        .arg(r.number(), returns::statusDisplay(r.status).toLower());
            return e;
        }
    }
    if (available == 0) {
        e.why = QStringLiteral("Everything in this order has already been returned.");
        return e;
    }
    if (e.late && !session.isAdmin()) {
        e.why = settings.value().windowDays == 0
                    ? QStringLiteral("This shop does not take returns after delivery. Please contact the shop.")
                    : QStringLiteral("Returns for this order closed on %1. Please contact the shop.").arg(dayText(e.lastDay));
        return e;
    }
    e.allowed = true;
    return e;
}

Result<ReturnRequest> ReturnService::request(const Session &session, qint64 orderId, const ReturnInput &input)
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;

    const QString note = cleanNote(input.note);
    if (note.size() > returns::kMaxNoteLength)
        return Result<ReturnRequest>::fail(QStringLiteral("Please keep the note under %1 characters.").arg(returns::kMaxNoteLength));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    auto eligible = eligibility(session, orderId);
    if (!eligible)
        return eligible.status();
    const ReturnEligibility &e = eligible.value();
    if (!e.allowed)
        return Result<ReturnRequest>::fail(e.why);
    if (input.reason == ReturnReason::ChangedMind && !e.changedMindAllowed)
        return Result<ReturnRequest>::fail(
            QStringLiteral("This shop does not take returns for a change of mind. Please choose another reason."));

    auto order = loadOrder(session, orderId);
    if (!order)
        return order.status();

    ReturnRequest r;
    r.orderId = orderId;
    r.userId = order.value().userId;
    r.reason = input.reason;
    r.note = note;
    r.late = e.late;
    r.status = session.isAdmin() ? ReturnStatus::Approved : ReturnStatus::Requested;
    if (session.isAdmin())
        r.decisionNote = e.late ? QStringLiteral("Opened by the shop after the return window") : QStringLiteral("Opened by the shop");

    QSet<qint64> seen;
    for (const auto &wanted : input.lines) {
        if (wanted.second <= 0)
            continue;
        if (seen.contains(wanted.first))
            return Result<ReturnRequest>::fail(QStringLiteral("Each product can be listed only once."));
        seen.insert(wanted.first);
        const ReturnableItem *match = nullptr;
        for (const ReturnableItem &item : e.items) {
            if (item.line.orderItemId == wanted.first)
                match = &item;
        }
        if (!match)
            return Result<ReturnRequest>::fail(QStringLiteral("That product is not part of order %1.").arg(order.value().orderNumber));
        if (wanted.second > match->available())
            return Result<ReturnRequest>::fail(
                match->available() == 0
                    ? QStringLiteral("%1 has already been returned.").arg(match->line.productName)
                    : QStringLiteral("You can return at most %1 of %2.").arg(match->available()).arg(match->line.productName));
        ReturnLine line = match->line;
        line.quantity = wanted.second;
        r.lines.push_back(line);
    }
    if (r.lines.isEmpty())
        return Result<ReturnRequest>::fail(QStringLiteral("Choose at least one item to return."));

    auto id = m_repos.returns.insert(r, session.userId());
    if (!id)
        return id.status();
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("return_requested"), QStringLiteral("return"), id.value(),
                                    QStringLiteral("order %1, %2").arg(order.value().orderNumber, returns::reasonToDb(r.reason)));
        !st)
        return st;
    auto saved = m_repos.returns.findById(id.value());
    if (!saved)
        return saved.status();
    if (auto st = tx.commit(); !st)
        return st;
    qInfo().noquote() << "Return" << returns::numberFor(id.value()) << "opened for order" << order.value().orderNumber;
    return *saved.value();
}

// ---------------------------------------------------------------- deciding

Status ReturnService::decide(const Session &session, qint64 returnId, ReturnStatus to, const QString &note,
                             const char *action)
{
    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto found = loadReturn(session, returnId);
    if (!found)
        return found.status();
    const ReturnRequest &r = found.value();
    if (!returns::canTransition(r.status, to)) {
        if (r.status == to)
            return Status::fail(QStringLiteral("Return %1 is already %2.").arg(r.number(), returns::statusDisplay(to).toLower()));
        return Status::fail(QStringLiteral("Return %1 is %2 and can no longer be changed this way.")
                                .arg(r.number(), returns::statusDisplay(r.status).toLower()));
    }
    if (auto st = m_repos.returns.setStatus(returnId, to, session.userId(), note); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QString::fromLatin1(action), QStringLiteral("return"), returnId,
                                    QStringLiteral("order %1").arg(r.orderNumber));
        !st)
        return st;
    return tx.commit();
}

Status ReturnService::approve(const Session &session, qint64 returnId, const QString &note)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    const QString text = cleanNote(note);
    if (text.size() > returns::kMaxNoteLength)
        return Status::fail(QStringLiteral("Please keep the note under %1 characters.").arg(returns::kMaxNoteLength));
    return decide(session, returnId, ReturnStatus::Approved, text, "return_approved");
}

Status ReturnService::reject(const Session &session, qint64 returnId, const QString &reason)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    const QString text = cleanNote(reason);
    if (text.isEmpty())
        return Status::fail(QStringLiteral("Tell the customer why the return is rejected."));
    if (text.size() > returns::kMaxNoteLength)
        return Status::fail(QStringLiteral("Please keep the reason under %1 characters.").arg(returns::kMaxNoteLength));
    return decide(session, returnId, ReturnStatus::Rejected, text, "return_rejected");
}

Status ReturnService::cancel(const Session &session, qint64 returnId, const QString &note)
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = loadReturn(session, returnId);
    if (!found)
        return found.status();
    if (!session.isAdmin() && found.value().status != ReturnStatus::Requested)
        return Status::fail(found.value().status == ReturnStatus::Approved
                                ? QStringLiteral("The shop has already approved this return. Please contact the shop to cancel it.")
                                : QStringLiteral("This return can no longer be cancelled."));
    QString text = cleanNote(note);
    if (text.size() > returns::kMaxNoteLength)
        return Status::fail(QStringLiteral("Please keep the note under %1 characters.").arg(returns::kMaxNoteLength));
    if (text.isEmpty())
        text = session.isAdmin() ? QStringLiteral("Cancelled by the shop") : QStringLiteral("Cancelled by the customer");
    return decide(session, returnId, ReturnStatus::Cancelled, text, "return_cancelled");
}

// ---------------------------------------------------------------- receiving

Result<RefundPreview> ReturnService::computeRefund(const ReturnRequest &r, const Order &o, const ReceiveInput &input,
                                                   std::optional<QistAccount> &qist) const
{
    RefundPreview p;
    p.shopFault = returns::isShopFault(r.reason);

    // Which ways of settling fit the order.
    if (o.paymentMethod == payment::kKhata) {
        p.methods = {RefundMethod::Khata};
        p.suggestedMethod = RefundMethod::Khata;
    } else if (o.paymentMethod == payment::kQist) {
        p.methods = {RefundMethod::Qist};
        p.suggestedMethod = RefundMethod::Qist;
    } else {
        p.methods = {RefundMethod::Cash, RefundMethod::Easypaisa, RefundMethod::JazzCash, RefundMethod::Bank,
                     RefundMethod::Khata};
        p.suggestedMethod = RefundMethod::Cash;
    }

    // Each line at the price paid, with its share of the order's sales tax.
    auto credited = m_repos.returns.creditedLines(o.id);
    if (!credited)
        return credited.status();
    QVector<Money> values;
    for (const OrderItem &item : o.items)
        values << item.lineTotal;
    const QVector<Money> lineTaxes = invoice::allocateTax(values, o.tax);
    for (const ReturnLine &line : r.lines) {
        ReturnLine out = line;
        for (int i = 0; i < o.items.size(); ++i) {
            if (o.items[i].id != line.orderItemId)
                continue;
            const auto before = credited.value().value(line.orderItemId);
            out.creditedTax = returns::taxForUnits(lineTaxes[i], o.items[i].quantity, line.quantity, before.units, before.tax);
        }
        for (const ReceiveLine &in : input.lines) {
            if (in.orderItemId == line.orderItemId) {
                out.restocked = in.restocked;
                out.damaged = in.damaged;
            }
        }
        p.goods += out.value();
        p.tax += out.creditedTax;
        p.lines.push_back(out);
    }

    // The delivery charge, once per order.
    auto refunded = m_repos.returns.refundedForOrder(o.id);
    if (!refunded)
        return refunded.status();
    p.shippingAvailable = o.shipping > refunded.value().first ? o.shipping - refunded.value().first : Money();
    p.shipping = input.refundShipping ? p.shippingAvailable : Money();
    p.total = p.goods + p.tax + p.shipping;

    // Qist: the returned share of the markup comes off too, and money already
    // paid beyond the new price goes back to the customer.
    const RefundMethod method = p.methods.contains(input.method) ? input.method : p.suggestedMethod;
    if (method == RefundMethod::Qist) {
        auto found = m_repos.qist.findByOrder(o.id);
        if (!found)
            return found.status();
        if (!found.value() || found.value()->state == QistState::Cancelled) {
            p.blocked = QStringLiteral("The qist for order %1 is cancelled, so nothing can be taken off it.").arg(o.orderNumber);
            return p;
        }
        qist = *found.value();
        const Money markupLeft = qist->markup > refunded.value().second ? qist->markup - refunded.value().second : Money();
        if (qist->cashPrice > Money()) {
            const qint64 share = (qist->markup.paisa() * p.total.paisa() * 2 + qist->cashPrice.paisa())
                                 / (2 * qist->cashPrice.paisa());
            p.markup = Money::fromPaisa(share) < markupLeft ? Money::fromPaisa(share) : markupLeft;
        }
        const Money newPrice = qist->effectivePrice() - p.total - p.markup;
        p.cashBack = qist->paid > newPrice ? qist->paid - newPrice : Money();
        const Money leftAfter = newPrice > qist->paid ? newPrice - qist->paid : Money();
        p.effect = QStringLiteral("%1 comes off the qist (%2 of it markup). %3")
                       .arg((p.total + p.markup).toString(), p.markup.toString(),
                            p.cashBack > Money()
                                ? QStringLiteral("The customer already paid more than the new price: pay back %1.").arg(p.cashBack.toString())
                                : leftAfter.isZero() ? QStringLiteral("Nothing is left to pay; the qist is complete.")
                                                     : QStringLiteral("%1 is left to pay.").arg(leftAfter.toString()));
    } else if (method == RefundMethod::Khata) {
        p.effect = QStringLiteral("%1 comes off %2's khata.").arg(p.total.toString(), r.customerName);
    } else {
        p.cashBack = p.total;
        p.effect = QStringLiteral("Pay %1 back to %2 by %3.")
                       .arg(p.total.toString(), r.customerName,
                            method == RefundMethod::Bank ? QStringLiteral("bank transfer") : returns::refundMethodDisplay(method));
    }

    // The credit note for the returned part.
    auto docs = m_repos.invoices.forOrder(o.id);
    if (!docs)
        return docs.status();
    if (docs.value().invoice) {
        p.invoiceNumber = docs.value().invoice->number;
        p.blocked = InvoiceService::creditNoteTooLate(*docs.value().invoice);
    }
    return p;
}

Result<RefundPreview> ReturnService::previewReceive(const Session &session, qint64 returnId, const ReceiveInput &input) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto found = loadReturn(session, returnId);
    if (!found)
        return found.status();
    auto order = loadOrder(session, found.value().orderId);
    if (!order)
        return order.status();
    std::optional<QistAccount> qist;
    return computeRefund(found.value(), order.value(), input, qist);
}

Result<ReturnRequest> ReturnService::receive(const Session &session, qint64 returnId, const ReceiveInput &input)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    const QString reference = input.reference.simplified();
    const QString note = cleanNote(input.note);
    if (reference.size() > returns::kMaxReferenceLength)
        return Result<ReturnRequest>::fail(QStringLiteral("The reference can be at most %1 characters.").arg(returns::kMaxReferenceLength));
    if (note.size() > returns::kMaxNoteLength)
        return Result<ReturnRequest>::fail(QStringLiteral("Please keep the note under %1 characters.").arg(returns::kMaxNoteLength));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    auto found = loadReturn(session, returnId);
    if (!found)
        return found.status();
    const ReturnRequest &r = found.value();
    if (r.status != ReturnStatus::Approved)
        return Result<ReturnRequest>::fail(r.status == ReturnStatus::Requested
                                               ? QStringLiteral("Approve return %1 before receiving it.").arg(r.number())
                                               : QStringLiteral("Return %1 is %2 and cannot be received.")
                                                     .arg(r.number(), returns::statusDisplay(r.status).toLower()));
    auto order = loadOrder(session, r.orderId);
    if (!order)
        return order.status();
    const Order &o = order.value();

    // Every line once, every unit accounted for.
    if (input.lines.size() != r.lines.size())
        return Result<ReturnRequest>::fail(QStringLiteral("Say for every item whether it goes back into stock or is damaged."));
    for (const ReturnLine &line : r.lines) {
        int matches = 0;
        for (const ReceiveLine &in : input.lines) {
            if (in.orderItemId != line.orderItemId)
                continue;
            ++matches;
            if (in.restocked < 0 || in.damaged < 0 || in.restocked + in.damaged != line.quantity)
                return Result<ReturnRequest>::fail(QStringLiteral("%1: %2 unit(s) came back. Split them between back in stock and damaged.")
                                                       .arg(line.productName)
                                                       .arg(line.quantity));
            if (in.restocked > 0 && !line.productId)
                return Result<ReturnRequest>::fail(QStringLiteral("%1 is no longer in the catalog, so it cannot go back into stock.")
                                                       .arg(line.productName));
        }
        if (matches != 1)
            return Result<ReturnRequest>::fail(QStringLiteral("Say for every item whether it goes back into stock or is damaged."));
    }

    std::optional<QistAccount> qist;
    auto computed = computeRefund(r, o, input, qist);
    if (!computed)
        return computed.status();
    const RefundPreview &p = computed.value();
    if (!p.blocked.isEmpty())
        return Result<ReturnRequest>::fail(p.blocked);
    if (!p.methods.contains(input.method))
        return Result<ReturnRequest>::fail(
            o.paymentMethod == payment::kKhata ? QStringLiteral("Order %1 was on khata, so the refund goes to the khata.").arg(o.orderNumber)
            : o.paymentMethod == payment::kQist ? QStringLiteral("Order %1 is on qist, so the refund comes off the qist.").arg(o.orderNumber)
                                                : QStringLiteral("Choose how the money is paid back."));
    if (returns::needsReference(input.method) && reference.isEmpty())
        return Result<ReturnRequest>::fail(QStringLiteral("Enter the %1 transaction number, so the refund can be traced.")
                                               .arg(returns::refundMethodDisplay(input.method)));

    // 1. Received
    if (auto st = m_repos.returns.setStatusOnly(returnId, ReturnStatus::Received); !st)
        return st;

    // 2. The goods: back on the shelf, or recorded as damaged
    for (const ReturnLine &line : p.lines) {
        if (line.restocked > 0) {
            auto newStock = m_repos.products.changeStock(*line.productId, line.restocked);
            if (!newStock)
                return newStock.status();
            if (auto st = m_repos.products.insertMovement(*line.productId, line.restocked, newStock.value(),
                                                          StockReason::ReturnRestock, r.number(), session.userId());
                !st)
                return st;
        }
        if (auto st = m_repos.returns.insertReceipt(returnId, line); !st)
            return st;
    }

    // 3. The credit note for the returned part
    ReturnRefund refund;
    refund.method = input.method;
    refund.reference = reference;
    refund.goods = p.goods;
    refund.tax = p.tax;
    refund.shipping = p.shipping;
    refund.total = p.total;
    refund.markup = p.markup;
    refund.cashBack = p.cashBack;
    refund.note = note;
    auto cn = InvoiceService::creditNoteForReturn(m_repos, o.id, returnId, r.number(), p.lines, p.shipping,
                                                  returns::reasonDisplay(r.reason), session.userId());
    if (!cn)
        return cn.status();
    if (cn.value())
        refund.creditNoteId = cn.value()->id;

    // 4. The money
    const QString tag = QStringLiteral("Return %1").arg(r.number());
    if (input.method == RefundMethod::Khata) {
        auto after = m_repos.khata.append(o.userId, KhataKind::Adjustment, Money() - p.total, o.id,
                                          QStringLiteral("%1: %2 credited").arg(tag, p.total.toString()), session.userId());
        if (!after)
            return after.status();
    } else if (input.method == RefundMethod::Qist && qist) {
        const Money cut = p.total + p.markup;
        auto after = m_repos.khata.append(o.userId, KhataKind::Adjustment, Money() - cut, o.id,
                                          QStringLiteral("%1: %2 taken off the qist").arg(tag, cut.toString()),
                                          session.userId(), QDateTime(), qist->id);
        if (!after)
            return after.status();
        if (p.cashBack > Money()) {
            auto back = m_repos.khata.append(o.userId, KhataKind::Adjustment, p.cashBack, o.id,
                                             QStringLiteral("%1: %2 paid back to the customer").arg(tag, p.cashBack.toString()),
                                             session.userId(), QDateTime(), qist->id);
            if (!back)
                return back.status();
        }
        const Money newPrice = qist->effectivePrice() - cut;
        if (qist->state == QistState::Active && qist->paid >= newPrice) {
            if (auto st = m_repos.qist.setState(qist->id, QistState::Completed); !st)
                return st;
            if (auto st = m_repos.orders.setPaymentStatus(o.id, payment::kPaid); !st)
                return st;
        }
    }
    // Cash, Easypaisa, JazzCash or bank: the shop pays it out; the refund row records it.

    // 5. Settled
    if (auto st = m_repos.returns.insertRefund(returnId, refund, session.userId()); !st)
        return st;
    if (auto st = m_repos.returns.setStatusOnly(returnId, ReturnStatus::Completed); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("return_received"), QStringLiteral("return"), returnId,
                                    QStringLiteral("order %1, %2 %3").arg(o.orderNumber, returns::refundMethodToDb(input.method),
                                                                         p.total.toPlainString()));
        !st)
        return st;
    auto saved = m_repos.returns.findById(returnId);
    if (!saved)
        return saved.status();
    if (auto st = tx.commit(); !st)
        return st;
    qInfo().noquote() << "Return" << r.number() << "received and settled";
    return *saved.value();
}

// ---------------------------------------------------------------- reading

Result<QVector<ReturnRequest>> ReturnService::list(const Session &session, const ReturnQuery &query) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return m_repos.returns.list(query);
}

Result<QVector<ReturnRequest>> ReturnService::forOrder(const Session &session, qint64 orderId) const
{
    auto order = loadOrder(session, orderId);
    if (!order)
        return order.status();
    return m_repos.returns.forOrder(orderId);
}

Result<ReturnRequest> ReturnService::details(const Session &session, qint64 returnId) const
{
    return loadReturn(session, returnId);
}

Result<int> ReturnService::waitingCount(const Session &session) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return m_repos.returns.countWithStatus(ReturnStatus::Requested);
}

} // namespace shophub
