#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

class PricingService;

// Tax invoices and credit notes, v2 milestone 4. See docs/INVOICES.md.
// - The admin fills in the shop's details (name, address, NTN, STRN, tax rate).
//   Until the name and address are saved, no invoice is made.
// - OrderService issues an order's invoice when it is marked Shipped, inside
//   the same transaction, and a credit note when a shipped parcel comes back.
// - Numbers follow on with no gaps (INV-2026-000001, CN-2026-000001); the
//   database refuses anything else and never lets an invoice change.
// - The verification code under the QR code is an HMAC-SHA256 signature made
//   with a secret key that ShopHub creates for the shop. The Verify screen
//   checks a printed invoice against it and against the invoice on record.
// ShopHub is not connected to FBR and never prints FBR's number, QR or logo.
class InvoiceService
{
public:
    static constexpr int kMaxTaxRateBp = 5000; // 50%

    InvoiceService(Repositories &repos, PricingService &pricing);

    // ---- shop details (admin)
    Result<ShopProfile> shopProfile(const Session &session) const;
    // Saves the details and the sales tax rate. The new rate applies to orders
    // placed from now on; orders and invoices already made keep theirs.
    Status saveShopProfile(const Session &session, ShopProfile profile);

    // ---- invoices
    Result<QVector<Invoice>> list(const Session &session, const InvoiceQuery &query) const;  // admin, no lines
    Result<InvoiceMonthSummary> monthSummary(const Session &session, int year, int month) const; // admin
    // Admin: the first day of every month that has an invoice or credit note, newest first.
    Result<QVector<QDate>> months(const Session &session) const;
    // Admin, or the customer whose order it is.
    Result<Invoice> invoice(const Session &session, qint64 invoiceId) const;
    Result<OrderInvoices> forOrder(const Session &session, qint64 orderId) const;

    // Admin: the invoice for a shipped or delivered order that has none yet
    // (for example one that shipped before the shop's details were filled in).
    Result<Invoice> issueInvoice(const Session &session, qint64 orderId);

    // Admin: is this paper genuine? `scanned` is the QR code's text (a USB QR
    // scanner types it) or just the invoice number, then `code` is the
    // verification code typed from the paper.
    Result<InvoiceVerification> verify(const Session &session, const QString &scanned, const QString &code) const;

    // ---- helpers for OrderService (they run inside the caller's transaction)
    static Result<ShopProfile> loadProfile(Repositories &repos);
    // Issues the order's invoice. Empty (and nothing saved) when the shop's
    // details are not filled in yet. Returns the existing one if there is one.
    static Result<std::optional<Invoice>> issueFor(Repositories &repos, const Order &order, qint64 userId);
    // The credit note cancelling the order's invoice. Empty when the order has
    // no invoice. Returns the existing one if there is one.
    static Result<std::optional<Invoice>> creditNoteFor(Repositories &repos, qint64 orderId, const QString &reason,
                                                        qint64 userId);

    // v3: a credit note for the items of a return only: these lines (value and
    // tax as credited), plus any delivery charge refunded. Empty when the order
    // has no invoice. Refused when the invoice is more than 180 days old.
    static Result<std::optional<Invoice>> creditNoteForReturn(Repositories &repos, qint64 orderId, qint64 returnId,
                                                              const QString &returnNumber,
                                                              const QVector<ReturnLine> &lines, Money shipping,
                                                              const QString &reason, qint64 userId);
    // Why a credit note can no longer be issued for this invoice (empty = it can).
    static QString creditNoteTooLate(const Invoice &invoice);

private:
    static Result<QByteArray> signingKey(Repositories &repos, bool create);
    static Result<QString> paymentTextFor(Repositories &repos, const Order &order);
    static void monthRange(int year, int month, QDateTime &fromUtc, QDateTime &toUtc);

    Repositories &m_repos;
    PricingService &m_pricing;
};

} // namespace shophub
