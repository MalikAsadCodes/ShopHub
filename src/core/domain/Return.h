#pragma once

#include "common/Money.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>

namespace shophub {

// Returns after delivery, ShopHub v3. See docs/RETURNS.md.
//
// A customer asks to send back some or all of a delivered order. The shop
// approves or rejects the request; approved goods come back and are then
// received and settled (v3 milestone 2).
//
//   requested -> approved -> received -> completed
//   requested -> rejected            (the shop says no, with a reason)
//   requested -> cancelled           (the customer changed their mind, or the shop)
//   approved  -> cancelled           (the goods never came back)
enum class ReturnStatus { Requested, Approved, Rejected, Cancelled, Received, Completed };

enum class ReturnReason { WrongItem, Damaged, Defective, NotAsDescribed, ChangedMind };

// How the money for a received return is settled (v3 milestone 2):
//   Cash, Easypaisa, JazzCash, Bank - paid back to the customer (Cash on Delivery orders)
//   Khata - credited to the customer's khata (a khata order always; a COD order if the customer prefers)
//   Qist  - taken off what is left to pay on the qist (a qist order always)
enum class RefundMethod { Cash, Easypaisa, JazzCash, Bank, Khata, Qist };

namespace returns {

constexpr int kDefaultWindowDays = 7;  // days after delivery the customer may ask
constexpr int kMaxWindowDays = 90;
constexpr int kMaxNoteLength = 300;

QString statusToDb(ReturnStatus s);                       // "requested"
std::optional<ReturnStatus> statusFromDb(const QString &s);
QString statusDisplay(ReturnStatus s);                    // "Waiting for review"
QString statusBadgeKind(ReturnStatus s);                  // "warning", "info", ...

QString reasonToDb(ReturnReason r);                       // "wrong_item"
std::optional<ReturnReason> reasonFromDb(const QString &s);
QString reasonDisplay(ReturnReason r);                    // "Wrong item sent"
QVector<ReturnReason> allReasons();

// Wrong, damaged, defective or not as described: the shop is at fault, so the
// delivery charge is refunded too (v3 milestone 2).
bool isShopFault(ReturnReason r);

// Still being handled: requested, approved or received. An order has at most one.
bool isOpen(ReturnStatus s);
bool canTransition(ReturnStatus from, ReturnStatus to);

// "RET-000012"
QString numberFor(qint64 returnId);

// The last day a customer may ask to return an order delivered at `deliveredUtc`.
QDate lastDay(const QDateTime &deliveredUtc, int windowDays);

constexpr int kMaxReferenceLength = 60;
// Pakistan's sales tax rules allow a credit note only within 180 days of the supply.
constexpr int kCreditNoteDays = 180;

QString refundMethodToDb(RefundMethod m);                 // "easypaisa"
std::optional<RefundMethod> refundMethodFromDb(const QString &s);
QString refundMethodDisplay(RefundMethod m);              // "Easypaisa"
// Money handed back to the customer (not kept on the khata or the qist).
bool paysBack(RefundMethod m);
// A transaction or cheque number is needed for these.
bool needsReference(RefundMethod m);

// The sales tax credited for `units` of an order line whose share of the order's
// tax is `lineTax` for `bought` units, when `creditedUnits` units of it were already
// credited for `creditedTax`. The last units take whatever tax is left, so a line
// never gets back more tax than it paid. Rounded half-up.
Money taxForUnits(Money lineTax, int bought, int units, int creditedUnits, Money creditedTax);

} // namespace returns

// One product line of a return: how many units of an order line come back.
struct ReturnLine
{
    qint64 orderItemId = 0;
    std::optional<qint64> productId; // empty if the product was later removed
    QString sku;
    QString productName;
    Money unitPrice;                 // the price paid (order line), wholesale or not
    int quantity = 0;
    // Once received (milestone 2): where the units went and what was credited.
    int restocked = 0;
    int damaged = 0;
    Money creditedTax;

    Money value() const { return unitPrice * quantity; }
};

// How a received return was settled.
struct ReturnRefund
{
    RefundMethod method = RefundMethod::Cash;
    QString reference;          // transaction or cheque number (may be empty for cash)
    Money goods;                // returned goods at the price paid, before tax
    Money tax;                  // their sales tax
    Money shipping;             // delivery charge refunded (shop's fault only)
    Money total;                // goods + tax + shipping: the value credited
    Money markup;               // qist: the returned share of the qist markup, also taken off
    Money cashBack;             // money handed back to the customer
    qint64 creditNoteId = 0;
    QString creditNoteNumber;   // empty if the order had no invoice
    QString note;
    QString receivedBy;
    QDateTime receivedAt;
};

struct ReturnRequest
{
    qint64 id = 0;
    qint64 orderId = 0;
    QString orderNumber;
    qint64 userId = 0;
    QString customerName;
    QString customerEmail;
    QString paymentMethod;      // of the order: "cod" | "khata" | "qist"
    ReturnStatus status = ReturnStatus::Requested;
    ReturnReason reason = ReturnReason::Defective;
    QString note;               // the customer's words
    QString decisionNote;       // the shop's answer (always given for a rejection)
    bool late = false;          // the shop accepted it after the return window
    QString requestedBy;        // who opened it (the customer, or the shop on their behalf)
    QString decidedBy;
    QDateTime createdAt;
    QDateTime decidedAt;        // invalid until approved, rejected or cancelled
    QDateTime updatedAt;
    int itemCount = 0;          // units, for lists
    Money value;                // price paid for the returned units, for lists
    QVector<ReturnLine> lines;  // filled in by details() and forOrder()
    std::optional<ReturnRefund> refund; // once received and settled

    QString number() const { return returns::numberFor(id); }
};

// What a return form starts from: each order line and how many units can still go back.
struct ReturnableItem
{
    ReturnLine line;            // quantity = units bought
    int alreadyReturned = 0;    // in returns that are open or done
    int available() const { return qMax(0, line.quantity - alreadyReturned); }
};

struct ReturnEligibility
{
    bool allowed = false;       // the signed-in user may open a return now
    QString why;                // when not allowed, a sentence for the user
    QDateTime deliveredAt;      // invalid if the order was never delivered
    QDate lastDay;              // the end of the return window
    bool late = false;          // past the window (only the shop may still open one)
    bool changedMindAllowed = true;
    QVector<ReturnableItem> items;
};

struct ReturnSettings
{
    int windowDays = returns::kDefaultWindowDays;
    bool allowChangedMind = true;
};

// What the customer (or the shop) asks to send back.
struct ReturnInput
{
    QVector<QPair<qint64, int>> lines; // order item id, units
    ReturnReason reason = ReturnReason::Defective;
    QString note;
};

// Receiving an approved return: where each unit goes, and how to settle the money.
struct ReceiveLine
{
    qint64 orderItemId = 0;
    int restocked = 0;          // back on the shelf (stock goes up)
    int damaged = 0;            // not sellable (stock stays as it is)
};

struct ReceiveInput
{
    QVector<ReceiveLine> lines; // every line of the return, once
    RefundMethod method = RefundMethod::Cash;
    QString reference;
    bool refundShipping = false;
    QString note;
};

// What receiving would do, worked out before anything is saved (the form shows it live).
struct RefundPreview
{
    QVector<ReturnLine> lines;          // with creditedTax
    Money goods;
    Money tax;
    Money shipping;                     // refunded now
    Money shippingAvailable;            // the order's delivery charge not refunded yet
    Money total;
    Money markup;                       // qist only
    Money cashBack;
    QVector<RefundMethod> methods;      // allowed for this order
    RefundMethod suggestedMethod = RefundMethod::Cash;
    bool shopFault = false;             // so the delivery charge is offered by default
    QString invoiceNumber;              // the order's invoice, if any: a credit note will be issued
    QString blocked;                    // why it cannot be received now (empty = fine)
    QString effect;                     // one sentence: what happens to the money
};

// Admin list of returns.
struct ReturnQuery
{
    enum class Show { Open, Requested, Approved, Closed, All };
    Show show = Show::Open;     // Open = requested, approved or received
    QString search;             // return or order number, customer name or email
    qint64 userId = 0;          // 0 = every customer
};

} // namespace shophub
