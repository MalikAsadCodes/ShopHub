#include "services/InvoiceService.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"
#include "database/Transaction.h"
#include "services/Access.h"
#include "services/PricingService.h"

#include <QLocale>
#include <QRandomGenerator>
#include <QtDebug>

#include <algorithm>

namespace shophub {

namespace {

// app_settings keys
const QString kShopName = QStringLiteral("shop_name");
const QString kShopAddress = QStringLiteral("shop_address");
const QString kShopPhone = QStringLiteral("shop_phone");
const QString kShopNtn = QStringLiteral("shop_ntn");
const QString kShopStrn = QStringLiteral("shop_strn");
const QString kTaxRate = QStringLiteral("tax_rate_bp");
const QString kSigningKey = QStringLiteral("invoice_signing_key");

QString dayText(const QDateTime &utc)
{
    return QLocale(QLocale::English).toString(utc.toLocalTime().date(), QStringLiteral("d MMM yyyy"));
}

QString kindName(const Invoice &inv)
{
    return inv.isCreditNote() ? QStringLiteral("Credit note") : QStringLiteral("Invoice");
}

} // namespace

InvoiceService::InvoiceService(Repositories &repos, PricingService &pricing)
    : m_repos(repos), m_pricing(pricing)
{
}

// ---------------------------------------------------------------- shop details

Result<ShopProfile> InvoiceService::loadProfile(Repositories &repos)
{
    ShopProfile p;
    const QVector<QPair<QString, QString *>> fields = {
        {kShopName, &p.name}, {kShopAddress, &p.address}, {kShopPhone, &p.phone}, {kShopNtn, &p.ntn}, {kShopStrn, &p.strn},
    };
    for (const auto &field : fields) {
        auto value = repos.settings.get(field.first);
        if (!value)
            return value.status();
        *field.second = value.value();
    }
    auto rate = repos.settings.get(kTaxRate, QStringLiteral("0"));
    if (!rate)
        return rate.status();
    bool ok = false;
    const int bp = rate.value().toInt(&ok);
    p.taxRateBp = ok ? qBound(0, bp, 10000) : 0;
    return p;
}

Result<ShopProfile> InvoiceService::shopProfile(const Session &session) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return loadProfile(m_repos);
}

Status InvoiceService::saveShopProfile(const Session &session, ShopProfile p)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    p.name = p.name.simplified();
    p.address = p.address.simplified();
    p.phone = p.phone.simplified();
    p.ntn = invoice::normalizeTaxNumber(p.ntn);
    p.strn = invoice::normalizeTaxNumber(p.strn);

    if (auto st = validation::checkRequired(p.name, QStringLiteral("Shop name"), 100); !st)
        return st;
    if (auto st = validation::checkRequired(p.address, QStringLiteral("Address"), 250); !st)
        return st;
    if (!p.phone.isEmpty() && !validation::isValidPhone(p.phone))
        return Status::fail(QStringLiteral("Please enter a valid phone number (e.g. 042 1234567), or leave it empty."));
    if (!p.ntn.isEmpty() && !invoice::isValidTaxNumber(p.ntn))
        return Status::fail(QStringLiteral("The NTN should be 7 to 17 digits (dashes and spaces are fine), or leave it empty."));
    if (!p.strn.isEmpty() && !invoice::isValidTaxNumber(p.strn))
        return Status::fail(QStringLiteral("The STRN should be 7 to 17 digits (dashes and spaces are fine), or leave it empty."));
    if (p.taxRateBp < 0 || p.taxRateBp > kMaxTaxRateBp)
        return Status::fail(QStringLiteral("The sales tax rate must be between 0% and %1%.").arg(kMaxTaxRateBp / 100));

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    const QVector<QPair<QString, QString>> values = {
        {kShopName, p.name}, {kShopAddress, p.address}, {kShopPhone, p.phone},
        {kShopNtn, p.ntn},   {kShopStrn, p.strn},       {kTaxRate, QString::number(p.taxRateBp)},
    };
    for (const auto &v : values) {
        if (auto st = m_repos.settings.set(v.first, v.second); !st)
            return st;
    }
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("shop_details_saved"), QStringLiteral("settings"), 0,
                                    QStringLiteral("sales tax %1%").arg(pricetier::percentText(p.taxRateBp)));
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;

    PricingRules rules = m_pricing.rules();
    rules.taxRateBp = p.taxRateBp;
    m_pricing.setRules(rules);
    qInfo().noquote() << "Shop details saved; sales tax rate" << pricetier::percentText(p.taxRateBp) + QLatin1Char('%');
    return Status::ok();
}

// ---------------------------------------------------------------- issuing

Result<QByteArray> InvoiceService::signingKey(Repositories &repos, bool create)
{
    auto stored = repos.settings.get(kSigningKey);
    if (!stored)
        return stored.status();
    const QByteArray key = QByteArray::fromHex(stored.value().toLatin1());
    if (key.size() == 32)
        return key;
    if (!create)
        return QByteArray();

    // 256 random bits from the operating system's secure generator.
    quint32 words[8];
    QRandomGenerator::system()->fillRange(words);
    QByteArray fresh(reinterpret_cast<const char *>(words), int(sizeof(words)));
    if (auto st = repos.settings.set(kSigningKey, QString::fromLatin1(fresh.toHex())); !st)
        return st;
    qInfo() << "Created the shop's invoice signing key";
    return fresh;
}

Result<QString> InvoiceService::paymentTextFor(Repositories &repos, const Order &order)
{
    if (order.paymentMethod == payment::kKhata)
        return QStringLiteral("Khata (on credit)");
    if (order.paymentMethod == payment::kQist) {
        auto found = repos.qist.findByOrder(order.id);
        if (!found)
            return found.status();
        if (found.value())
            return QStringLiteral("Qist (installments): %1").arg(found.value()->planName);
        return payment::methodDisplay(payment::kQist);
    }
    return payment::methodDisplay(payment::kCashOnDelivery);
}

Result<std::optional<Invoice>> InvoiceService::issueFor(Repositories &repos, const Order &orderIn, qint64 userId)
{
    auto profile = loadProfile(repos);
    if (!profile)
        return profile.status();
    if (!profile.value().isComplete()) {
        qInfo().noquote() << "No invoice for" << orderIn.orderNumber << "- the shop's details are not filled in yet";
        return std::optional<Invoice>{};
    }
    auto existing = repos.invoices.forOrder(orderIn.id);
    if (!existing)
        return existing.status();
    if (existing.value().invoice)
        return existing.value().invoice;

    Order order = orderIn;
    if (order.items.isEmpty()) {
        auto full = repos.orders.findById(order.id, true);
        if (!full)
            return full.status();
        if (!full.value())
            return Result<std::optional<Invoice>>::fail(QStringLiteral("Order not found."));
        order = *full.value();
    }
    auto key = signingKey(repos, true);
    if (!key)
        return key.status();
    auto seq = repos.invoices.nextSeq(InvoiceKind::Invoice);
    if (!seq)
        return seq.status();
    auto paymentText = paymentTextFor(repos, order);
    if (!paymentText)
        return paymentText.status();

    const ShopProfile &shop = profile.value();
    Invoice inv;
    inv.kind = InvoiceKind::Invoice;
    inv.seq = seq.value();
    inv.issuedAt = sql::fromDb(sql::nowUtc());
    inv.number = invoice::formatNumber(inv.kind, inv.issuedDate().year(), inv.seq);
    inv.title = shop.registeredForSalesTax() ? invoice::kTaxInvoiceTitle : invoice::kInvoiceTitle;
    inv.orderId = order.id;
    inv.shop = shop;
    inv.buyerName = order.shipName;
    inv.buyerPhone = order.shipPhone;
    inv.buyerAddress = order.shipLine1;
    inv.buyerCity = order.shipCity;
    inv.buyerTaxNumber = order.buyerTaxNumber;
    inv.paymentText = paymentText.value();
    inv.goods = order.subtotal;
    inv.shipping = order.shipping;
    inv.tax = order.tax;
    inv.total = order.total;
    inv.taxRateBp = order.taxRateBp.value_or(invoice::deriveRateBp(order.subtotal, order.tax));

    QVector<Money> values;
    for (const OrderItem &item : order.items)
        values << item.lineTotal;
    const QVector<Money> taxes = invoice::allocateTax(values, order.tax);
    for (int i = 0; i < order.items.size(); ++i) {
        const OrderItem &item = order.items[i];
        InvoiceLine line;
        line.lineNo = i + 1;
        line.sku = item.sku;
        line.description = item.productName;
        line.quantity = item.quantity;
        line.unitPrice = item.unitPrice;
        line.value = item.lineTotal;
        line.tax = taxes[i];
        inv.lines.push_back(line);
    }
    inv.code = invoice::signatureCode(key.value(), inv.kind, inv.number, inv.total);

    auto id = repos.invoices.insert(inv, userId);
    if (!id)
        return id.status();
    if (auto st = repos.audit.log(userId, QStringLiteral("invoice_issued"), QStringLiteral("invoice"), id.value(),
                                  QStringLiteral("%1 for order %2").arg(inv.number, order.orderNumber));
        !st)
        return st;
    qInfo().noquote() << "Invoice" << inv.number << "issued for order" << order.orderNumber;
    auto saved = repos.invoices.findById(id.value());
    if (!saved)
        return saved.status();
    return saved.value();
}

Result<std::optional<Invoice>> InvoiceService::creditNoteFor(Repositories &repos, qint64 orderId, const QString &reason,
                                                             qint64 userId)
{
    auto existing = repos.invoices.forOrder(orderId);
    if (!existing)
        return existing.status();
    if (!existing.value().invoice)
        return std::optional<Invoice>{};
    if (existing.value().creditNote)
        return existing.value().creditNote;

    auto key = signingKey(repos, true);
    if (!key)
        return key.status();
    auto seq = repos.invoices.nextSeq(InvoiceKind::CreditNote);
    if (!seq)
        return seq.status();

    // The same sale, the same buyer and the same amounts, cancelled.
    const Invoice &original = *existing.value().invoice;
    Invoice cn = original;
    cn.id = 0;
    cn.kind = InvoiceKind::CreditNote;
    cn.seq = seq.value();
    cn.issuedAt = sql::fromDb(sql::nowUtc());
    cn.number = invoice::formatNumber(cn.kind, cn.issuedDate().year(), cn.seq);
    cn.title = invoice::kCreditNoteTitle;
    cn.creditForId = original.id;
    cn.reason = reason.simplified().isEmpty() ? QStringLiteral("Sale cancelled") : reason.simplified().left(200);
    cn.code = invoice::signatureCode(key.value(), cn.kind, cn.number, cn.total);

    auto id = repos.invoices.insert(cn, userId);
    if (!id)
        return id.status();
    if (auto st = repos.audit.log(userId, QStringLiteral("credit_note_issued"), QStringLiteral("invoice"), id.value(),
                                  QStringLiteral("%1 cancels %2").arg(cn.number, original.number));
        !st)
        return st;
    qInfo().noquote() << "Credit note" << cn.number << "issued for invoice" << original.number;
    auto saved = repos.invoices.findById(id.value());
    if (!saved)
        return saved.status();
    return saved.value();
}

QString InvoiceService::creditNoteTooLate(const Invoice &invoice)
{
    const QDate last = invoice.issuedDate().addDays(returns::kCreditNoteDays);
    if (sql::todayLocal() <= last)
        return QString();
    return QStringLiteral("Invoice %1 was issued on %2, more than %3 days ago, so a credit note can no longer be "
                          "issued for it.")
        .arg(invoice.number, dayText(invoice.issuedAt))
        .arg(returns::kCreditNoteDays);
}

Result<std::optional<Invoice>> InvoiceService::creditNoteForReturn(Repositories &repos, qint64 orderId, qint64 returnId,
                                                                   const QString &returnNumber,
                                                                   const QVector<ReturnLine> &lines, Money shipping,
                                                                   const QString &reason, qint64 userId)
{
    auto existing = repos.invoices.forOrder(orderId);
    if (!existing)
        return existing.status();
    if (!existing.value().invoice)
        return std::optional<Invoice>{};
    const Invoice &original = *existing.value().invoice;
    if (existing.value().creditNote)
        return Result<std::optional<Invoice>>::fail(
            QStringLiteral("Invoice %1 was already cancelled by credit note %2.").arg(original.number, existing.value().creditNote->number));
    if (const QString late = creditNoteTooLate(original); !late.isEmpty())
        return Result<std::optional<Invoice>>::fail(late);

    auto key = signingKey(repos, true);
    if (!key)
        return key.status();
    auto seq = repos.invoices.nextSeq(InvoiceKind::CreditNote);
    if (!seq)
        return seq.status();

    // The same seller, buyer and payment as the invoice; only the returned lines.
    Invoice cn = original;
    cn.id = 0;
    cn.kind = InvoiceKind::CreditNote;
    cn.seq = seq.value();
    cn.issuedAt = sql::fromDb(sql::nowUtc());
    cn.number = invoice::formatNumber(cn.kind, cn.issuedDate().year(), cn.seq);
    cn.title = invoice::kCreditNoteTitle;
    cn.creditForId = original.id;
    cn.returnId = returnId;
    cn.reason = QStringLiteral("Return %1: %2").arg(returnNumber, reason.simplified()).left(200);
    cn.lines.clear();
    cn.goods = Money();
    cn.tax = Money();
    int lineNo = 0;
    for (const ReturnLine &l : lines) {
        InvoiceLine line;
        line.lineNo = ++lineNo;
        line.sku = l.sku;
        line.description = l.productName;
        line.quantity = l.quantity;
        line.unitPrice = l.unitPrice;
        line.value = l.value();
        line.tax = l.creditedTax;
        cn.goods += line.value;
        cn.tax += line.tax;
        cn.lines.push_back(line);
    }
    cn.shipping = shipping;
    cn.total = cn.goods + cn.shipping + cn.tax;
    cn.code = invoice::signatureCode(key.value(), cn.kind, cn.number, cn.total);

    auto id = repos.invoices.insert(cn, userId);
    if (!id)
        return id.status();
    if (auto st = repos.audit.log(userId, QStringLiteral("credit_note_issued"), QStringLiteral("invoice"), id.value(),
                                  QStringLiteral("%1 for return %2 on %3").arg(cn.number, returnNumber, original.number));
        !st)
        return st;
    qInfo().noquote() << "Credit note" << cn.number << "issued for return" << returnNumber;
    auto saved = repos.invoices.findById(id.value());
    if (!saved)
        return saved.status();
    return saved.value();
}

Result<Invoice> InvoiceService::issueInvoice(const Session &session, qint64 orderId)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    auto order = m_repos.orders.findById(orderId, true);
    if (!order)
        return order.status();
    if (!order.value())
        return Result<Invoice>::fail(QStringLiteral("Order not found."));
    const Order &o = *order.value();
    if (o.status != OrderStatus::Shipped && o.status != OrderStatus::Delivered)
        return Result<Invoice>::fail(QStringLiteral("An invoice is made when the order ships. Mark order %1 as shipped first.")
                                         .arg(o.orderNumber));
    if (!o.invoiceNumber.isEmpty())
        return Result<Invoice>::fail(QStringLiteral("Order %1 already has invoice %2.").arg(o.orderNumber, o.invoiceNumber));
    auto profile = loadProfile(m_repos);
    if (!profile)
        return profile.status();
    if (!profile.value().isComplete())
        return Result<Invoice>::fail(QStringLiteral("Fill in your shop's name and address first (Invoices > Shop details)."));

    auto issued = issueFor(m_repos, o, session.userId());
    if (!issued)
        return issued.status();
    if (!issued.value())
        return Result<Invoice>::fail(sql::kGenericDbError);
    if (auto st = tx.commit(); !st)
        return st;
    return *issued.value();
}

// ---------------------------------------------------------------- reading

void InvoiceService::monthRange(int year, int month, QDateTime &fromUtc, QDateTime &toUtc)
{
    // Months are the shop's local months: an invoice issued at 1 am on the 1st
    // belongs to the new month, even though it is still the old month in UTC.
    const QDate first(year, month, 1);
    fromUtc = QDateTime(first, QTime(0, 0)).toUTC();
    toUtc = QDateTime(first.addMonths(1), QTime(0, 0)).toUTC();
}

Result<QVector<Invoice>> InvoiceService::list(const Session &session, const InvoiceQuery &query) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    QDateTime from;
    QDateTime to;
    if (query.year > 0 && query.month >= 1 && query.month <= 12)
        monthRange(query.year, query.month, from, to);
    return m_repos.invoices.list(query, from, to);
}

Result<InvoiceMonthSummary> InvoiceService::monthSummary(const Session &session, int year, int month) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (year < 2000 || month < 1 || month > 12)
        return Result<InvoiceMonthSummary>::fail(QStringLiteral("Please choose a month."));
    InvoiceQuery q;
    q.year = year;
    q.month = month;
    auto rows = list(session, q);
    if (!rows)
        return rows.status();
    InvoiceMonthSummary s;
    s.year = year;
    s.month = month;
    for (const Invoice &inv : rows.value()) {
        if (inv.isCreditNote()) {
            ++s.creditNotes;
            s.creditGoods += inv.goods;
            s.creditTax += inv.tax;
            s.creditTotal += inv.total;
        } else {
            ++s.invoices;
            s.goods += inv.goods;
            s.tax += inv.tax;
            s.total += inv.total;
        }
    }
    return s;
}

Result<QVector<QDate>> InvoiceService::months(const Session &session) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto rows = m_repos.invoices.list(InvoiceQuery(), QDateTime(), QDateTime());
    if (!rows)
        return rows.status();
    QVector<QDate> out;
    for (const Invoice &inv : rows.value()) {
        const QDate d = inv.issuedDate();
        const QDate first(d.year(), d.month(), 1);
        if (!out.contains(first))
            out.push_back(first);
    }
    std::sort(out.begin(), out.end(), [](const QDate &a, const QDate &b) { return a > b; });
    return out;
}

Result<Invoice> InvoiceService::invoice(const Session &session, qint64 invoiceId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.invoices.findById(invoiceId);
    if (!found)
        return found.status();
    // Same message for "missing" and "someone else's".
    if (!found.value() || (!session.isAdmin() && found.value()->userId != session.userId()))
        return Result<Invoice>::fail(QStringLiteral("Invoice not found."));
    return *found.value();
}

Result<OrderInvoices> InvoiceService::forOrder(const Session &session, qint64 orderId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto order = m_repos.orders.findById(orderId, false);
    if (!order)
        return order.status();
    if (!order.value() || (!session.isAdmin() && order.value()->userId != session.userId()))
        return Result<OrderInvoices>::fail(QStringLiteral("Order not found."));
    return m_repos.invoices.forOrder(orderId);
}

// ---------------------------------------------------------------- verifying

Result<InvoiceVerification> InvoiceService::verify(const Session &session, const QString &scanned,
                                                   const QString &code) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    const QString input = scanned.trimmed();
    InvoiceVerification v;
    if (input.isEmpty())
        return Result<InvoiceVerification>::fail(QStringLiteral("Scan the QR code on the invoice, or type its number."));

    const std::optional<invoice::QrFields> qr = invoice::parseQrText(input);
    if (!qr && input.contains(QLatin1Char('|'))) {
        v.verdict = InvoiceVerification::Verdict::NotGenuine;
        v.headline = QStringLiteral("Not a ShopHub invoice");
        v.message = QStringLiteral("This QR code was not made by ShopHub.");
        return v;
    }
    const QString number = qr ? qr->number : input.toUpper();
    const QString givenCode = qr ? qr->code : code.trimmed();
    if (invoice::normalizeCode(givenCode).isEmpty())
        return Result<InvoiceVerification>::fail(
            QStringLiteral("Type the 16-character code printed under the QR code, or scan the QR code."));

    auto found = m_repos.invoices.findByNumber(number);
    if (!found)
        return found.status();

    if (found.value()) {
        const Invoice &inv = *found.value();
        v.invoice = inv;
        if (!invoice::sameCode(givenCode, inv.code)) {
            v.verdict = InvoiceVerification::Verdict::NotGenuine;
            v.headline = QStringLiteral("Not genuine");
            v.message = QStringLiteral("The code does not match %1 %2. This paper was not printed by this shop's "
                                       "ShopHub, or it was changed.")
                            .arg(kindName(inv).toLower(), inv.number);
            return v;
        }
        if (qr && qr->total != inv.total) {
            v.verdict = InvoiceVerification::Verdict::NotGenuine;
            v.headline = QStringLiteral("Not genuine");
            v.message = QStringLiteral("The amount in the QR code (%1) is not the amount of %2 (%3).")
                            .arg(qr->total.toString(), inv.number, inv.total.toString());
            return v;
        }
        v.verdict = InvoiceVerification::Verdict::Genuine;
        v.headline = QStringLiteral("Genuine");
        v.message = QStringLiteral("%1 %2 was issued by %3 on %4 to %5 for %6.")
                        .arg(kindName(inv), inv.number, inv.shop.name, dayText(inv.issuedAt), inv.buyerName,
                             inv.total.toString());
        if (inv.isCancelled())
            v.message += QStringLiteral(" It was cancelled later by credit note %1.").arg(inv.creditNoteNumber);
        if (inv.isReturnCredit())
            v.message += QStringLiteral(" It credits returned items on invoice %1.").arg(inv.creditForNumber);
        else if (inv.isCreditNote())
            v.message += QStringLiteral(" It cancels invoice %1.").arg(inv.creditForNumber);
        if (inv.isPartlyCredited())
            v.message += QStringLiteral(" Returned items were credited by %1.").arg(inv.returnCreditNotes.join(QStringLiteral(", ")));
        return v;
    }

    // Not on record. A scanned QR code can still be checked against the shop's key.
    v.verdict = InvoiceVerification::Verdict::NotGenuine;
    v.headline = QStringLiteral("Not genuine");
    v.message = QStringLiteral("There is no invoice or credit note %1 in ShopHub.").arg(number);
    if (qr) {
        auto key = signingKey(m_repos, false);
        if (!key)
            return key.status();
        if (!key.value().isEmpty()
            && invoice::sameCode(givenCode, invoice::signatureCode(key.value(), invoice::kindOfNumber(number), number, qr->total))) {
            v.verdict = InvoiceVerification::Verdict::NotFound;
            v.headline = QStringLiteral("Genuine code, not in this database");
            v.message = QStringLiteral("The code was made with this shop's key, but %1 is not in this database. "
                                       "It may come from a backup or another copy of ShopHub.")
                            .arg(number);
        } else {
            v.message += QStringLiteral(" The code is not genuine either.");
        }
    }
    return v;
}

} // namespace shophub
