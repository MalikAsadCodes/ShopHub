#pragma once

// Tax invoices and credit notes - v2 milestone 4. See docs/INVOICES.md.
//
// What an invoice holds, and the parts of invoicing that need no database:
// numbering, spreading the order's sales tax over its lines, the amount in
// words, checking tax numbers, and the verification code printed under the
// QR code (an HMAC-SHA256 signature made with the shop's own secret key).
//
// ShopHub's invoices follow section 23 of the Sales Tax Act 1990 (serial
// number, date, supplier and buyer, description, quantity, value before tax,
// sales tax, value with tax). They are NOT connected to FBR: no FBR invoice
// number, FBR QR code or FBR logo is ever printed.

#include "common/Money.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace shophub {

// The shop's own details, printed on every invoice (Admin > Invoices > Shop details).
struct ShopProfile
{
    QString name;
    QString address;
    QString phone;
    QString ntn;     // National Tax Number, optional
    QString strn;    // Sales Tax Registration Number, optional
    int taxRateBp = 0; // sales tax on new orders: 1800 = 18%

    // Invoices are made only once the shop has said who it is.
    bool isComplete() const { return !name.trimmed().isEmpty() && !address.trimmed().isEmpty(); }
    // Only a registered person may issue a *tax* invoice (section 23(2)).
    bool registeredForSalesTax() const { return !strn.trimmed().isEmpty(); }
};

enum class InvoiceKind { Invoice, CreditNote };

QString invoiceKindToDb(InvoiceKind kind);                 // "invoice" | "credit_note"
std::optional<InvoiceKind> invoiceKindFromDb(const QString &s);

// One line: what was sold, how many, and its value before and after tax.
struct InvoiceLine
{
    int lineNo = 0;
    QString sku;
    QString description;
    int quantity = 0;
    Money unitPrice; // before tax
    Money value;     // unitPrice x quantity, before tax
    Money tax;       // this line's share of the order's sales tax

    Money valueWithTax() const { return value + tax; }
};

// An invoice or a credit note. Everything printed on it is a copy taken when
// it was issued, so later changes (a renamed product, a new shop address)
// never change an invoice that was already given to a customer.
struct Invoice
{
    qint64 id = 0;
    InvoiceKind kind = InvoiceKind::Invoice;
    int seq = 0;             // 1, 2, 3 ... per kind, never skipped or repeated
    QString number;          // INV-2026-000001 or CN-2026-000001
    QString title;           // "Sales Tax Invoice", "Invoice" or "Credit Note"
    qint64 orderId = 0;
    QString orderNumber;     // looked up from the order
    qint64 userId = 0;       // the customer (from the order)
    QDateTime issuedAt;
    QString issuedBy;        // who issued it

    ShopProfile shop;        // as it was when issued (taxRateBp not used)
    QString buyerName;
    QString buyerPhone;
    QString buyerAddress;
    QString buyerCity;
    QString buyerTaxNumber;  // NTN or STRN of a business buyer, may be empty
    QString paymentText;     // "Cash on Delivery", "Khata", "Qist (installments): 6 months"
    QString reason;          // credit notes: why the sale was cancelled

    Money goods;             // value of the goods before tax
    Money shipping;          // delivery charge (no sales tax on it)
    int taxRateBp = 0;
    Money tax;
    Money total;             // goods + shipping + tax
    QString code;            // verification code printed under the QR code
    QVector<InvoiceLine> lines;

    // Links, looked up when the invoice is read.
    qint64 creditForId = 0;     // credit note: the invoice it cancels
    QString creditForNumber;
    QDateTime creditForIssuedAt;
    QString creditNoteNumber;   // invoice: the credit note that cancelled all of it, if any
    QStringList returnCreditNotes; // invoice: credit notes for returned items (part of it), oldest first
    qint64 returnId = 0;        // credit note for a return (v3): that return, else 0

    bool isCreditNote() const { return kind == InvoiceKind::CreditNote; }
    bool isCancelled() const { return !isCreditNote() && !creditNoteNumber.isEmpty(); }
    bool isPartlyCredited() const { return !isCreditNote() && !returnCreditNotes.isEmpty(); }
    bool isReturnCredit() const { return isCreditNote() && returnId > 0; }
    QDate issuedDate() const { return issuedAt.toLocalTime().date(); }
};

// An order's invoice and, if the sale was cancelled after it, its credit note.
// Credit notes for returned items (v3) are listed separately.
struct OrderInvoices
{
    std::optional<Invoice> invoice;
    std::optional<Invoice> creditNote;     // cancels the whole invoice
    QVector<Invoice> returnCreditNotes;    // one per return, oldest first
};

// Admin list of invoices.
struct InvoiceQuery
{
    enum class Show { All, Invoices, CreditNotes };
    Show show = Show::All;
    QString search;   // number, order number or customer name
    int year = 0;     // with month: only that month (local time), 0 = every month
    int month = 0;
    qint64 userId = 0; // 0 = every customer
    int limit = 5000;
};

// One month, for the monthly sales tax return.
struct InvoiceMonthSummary
{
    int year = 0;
    int month = 0;
    int invoices = 0;
    Money goods;          // invoices: value before tax
    Money tax;            // invoices: sales tax
    Money total;
    int creditNotes = 0;
    Money creditGoods;
    Money creditTax;
    Money creditTotal;

    Money netGoods() const { return goods - creditGoods; }
    Money netTax() const { return tax - creditTax; }
};

// What the Verify screen shows.
struct InvoiceVerification
{
    enum class Verdict {
        Genuine,      // made by this shop's ShopHub and unchanged
        NotGenuine,   // the code or the amount does not match
        NotFound,     // the code is genuine, but this invoice is not in this database
    };
    Verdict verdict = Verdict::NotGenuine;
    QString headline;  // "Genuine", "Not genuine", ...
    QString message;   // one or two sentences for the shopkeeper
    std::optional<Invoice> invoice;
};

namespace invoice {

inline const QString kTaxInvoiceTitle = QStringLiteral("Sales Tax Invoice");
inline const QString kInvoiceTitle = QStringLiteral("Invoice");
inline const QString kCreditNoteTitle = QStringLiteral("Credit Note");
inline const QString kQrPrefix = QStringLiteral("SHOPHUB");

// "INV-2026-000145" / "CN-2026-000003". The year is the year it was issued;
// the counter never restarts, so a number can never be used twice.
QString formatNumber(InvoiceKind kind, int year, int seq);
// Credit note numbers start with "CN-".
InvoiceKind kindOfNumber(const QString &number);

// Spreads `tax` over lines in proportion to their values, so that the line
// taxes add up to exactly `tax`. Leftover paisa go to the lines with the
// largest remainders (the first line on a tie). All zero if the values are.
QVector<Money> allocateTax(const QVector<Money> &values, Money tax);

// The rate an order was taxed at, for orders saved before the rate was stored:
// tax / goods in basis points, rounded half-up, between 0 and 10000.
int deriveRateBp(Money goods, Money tax);

// "Rupees Forty Three Thousand Four Hundred Thirteen and Fourteen Paisa Only",
// counted the Pakistani way (thousand, lakh, crore).
QString amountInWords(Money amount);

// An NTN, STRN or CNIC as people write them: 7 to 17 digits, with optional
// dashes or spaces between them. Empty is not valid (the field is optional,
// so check for empty first).
bool isValidTaxNumber(const QString &text);
QString normalizeTaxNumber(const QString &text); // trimmed, single spaces

// ---- verification code
// 16 characters in 4 groups (XXXX-XXXX-XXXX-XXXX): the first 80 bits of
// HMAC-SHA256(key, "ShopHub invoice v1|<kind>|<number>|<total in paisa>"),
// written in Crockford's base 32 (no I, L, O or U, so it is easy to type).
QString signatureCode(const QByteArray &key, InvoiceKind kind, const QString &number, Money total);
// Upper case, without spaces or dashes, with O read as 0 and I or L as 1.
// Empty if what is left is not 16 base-32 characters.
QString normalizeCode(const QString &code);
bool sameCode(const QString &a, const QString &b);

// What the QR code says: "SHOPHUB|INV-2026-000145|43413.14|ABCD-EFGH-JKMN-PQRS".
// A USB QR scanner types exactly this into the Verify screen.
QString qrText(const Invoice &invoice);

struct QrFields
{
    QString number;
    Money total;
    QString code;
};
// Empty if the text is not a ShopHub invoice QR code.
std::optional<QrFields> parseQrText(const QString &text);

} // namespace invoice
} // namespace shophub
