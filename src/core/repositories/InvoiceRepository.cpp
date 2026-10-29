#include "repositories/InvoiceRepository.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

namespace {

const QString kSelectInvoice = QStringLiteral(
    "SELECT i.id, i.kind, i.seq, i.number, i.title, i.order_id, o.order_number, o.user_id, i.credit_for, "
    "       i.issued_at, COALESCE(u.full_name, ''), i.shop_name, i.shop_address, i.shop_phone, i.shop_ntn, "
    "       i.shop_strn, i.buyer_name, i.buyer_phone, i.buyer_address, i.buyer_city, i.buyer_tax_number, "
    "       i.payment_text, i.reason, i.goods_paisa, i.shipping_paisa, i.tax_rate_bp, i.tax_paisa, "
    "       i.total_paisa, i.code, COALESCE(c.number, ''), c.issued_at, "
    "       COALESCE((SELECT n.number FROM invoices n WHERE n.credit_for = i.id AND n.kind = 'credit_note' "
    "                 AND n.return_id IS NULL), ''), "
    "       i.return_id, "
    "       COALESCE((SELECT group_concat(number, ',') FROM (SELECT n.number FROM invoices n WHERE n.credit_for = i.id "
    "                 AND n.kind = 'credit_note' AND n.return_id IS NOT NULL ORDER BY n.id)), '') "
    "FROM invoices i "
    "JOIN orders o ON o.id = i.order_id "
    "LEFT JOIN users u ON u.id = i.issued_by "
    "LEFT JOIN invoices c ON c.id = i.credit_for ");

Invoice readInvoice(const QSqlQuery &q)
{
    Invoice inv;
    inv.id = q.value(0).toLongLong();
    inv.kind = invoiceKindFromDb(q.value(1).toString()).value_or(InvoiceKind::Invoice);
    inv.seq = q.value(2).toInt();
    inv.number = q.value(3).toString();
    inv.title = q.value(4).toString();
    inv.orderId = q.value(5).toLongLong();
    inv.orderNumber = q.value(6).toString();
    inv.userId = q.value(7).toLongLong();
    inv.creditForId = q.value(8).isNull() ? 0 : q.value(8).toLongLong();
    inv.issuedAt = sql::fromDb(q.value(9));
    inv.issuedBy = q.value(10).toString();
    inv.shop.name = q.value(11).toString();
    inv.shop.address = q.value(12).toString();
    inv.shop.phone = q.value(13).toString();
    inv.shop.ntn = q.value(14).toString();
    inv.shop.strn = q.value(15).toString();
    inv.buyerName = q.value(16).toString();
    inv.buyerPhone = q.value(17).toString();
    inv.buyerAddress = q.value(18).toString();
    inv.buyerCity = q.value(19).toString();
    inv.buyerTaxNumber = q.value(20).toString();
    inv.paymentText = q.value(21).toString();
    inv.reason = q.value(22).toString();
    inv.goods = Money::fromPaisa(q.value(23).toLongLong());
    inv.shipping = Money::fromPaisa(q.value(24).toLongLong());
    inv.taxRateBp = q.value(25).toInt();
    inv.shop.taxRateBp = inv.taxRateBp;
    inv.tax = Money::fromPaisa(q.value(26).toLongLong());
    inv.total = Money::fromPaisa(q.value(27).toLongLong());
    inv.code = q.value(28).toString();
    inv.creditForNumber = q.value(29).toString();
    inv.creditForIssuedAt = sql::fromDb(q.value(30));
    inv.creditNoteNumber = q.value(31).toString();
    inv.returnId = q.value(32).isNull() ? 0 : q.value(32).toLongLong();
    const QString partial = q.value(33).toString();
    if (!partial.isEmpty())
        inv.returnCreditNotes = partial.split(QLatin1Char(','));
    return inv;
}

} // namespace

Result<int> InvoiceRepository::nextSeq(InvoiceKind kind) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COALESCE(MAX(seq), 0) + 1 FROM invoices WHERE kind = :k"));
    q.bindValue(QStringLiteral(":k"), invoiceKindToDb(kind));
    if (auto st = sql::exec(q, "InvoiceRepository::nextSeq"); !st)
        return st;
    q.next();
    return q.value(0).toInt();
}

Result<qint64> InvoiceRepository::insert(const Invoice &inv, qint64 issuedBy)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO invoices (kind, seq, number, title, order_id, credit_for, issued_at, issued_by, shop_name, "
        "  shop_address, shop_phone, shop_ntn, shop_strn, buyer_name, buyer_phone, buyer_address, buyer_city, "
        "  buyer_tax_number, payment_text, reason, goods_paisa, shipping_paisa, tax_rate_bp, tax_paisa, "
        "  total_paisa, code, return_id) "
        "VALUES (:kind, :seq, :num, :title, :ord, :cf, :at, :by, :sn, :sa, :sp, :sntn, :sstrn, :bn, :bp, :ba, "
        "  :bc, :btn, :pay, :why, :goods, :ship, :rate, :tax, :total, :code, :ret)"));
    q.bindValue(QStringLiteral(":kind"), invoiceKindToDb(inv.kind));
    q.bindValue(QStringLiteral(":seq"), inv.seq);
    q.bindValue(QStringLiteral(":num"), inv.number);
    q.bindValue(QStringLiteral(":title"), inv.title);
    q.bindValue(QStringLiteral(":ord"), inv.orderId);
    q.bindValue(QStringLiteral(":cf"), sql::nullable(inv.creditForId));
    q.bindValue(QStringLiteral(":at"), sql::toDb(inv.issuedAt));
    q.bindValue(QStringLiteral(":by"), sql::nullable(issuedBy));
    q.bindValue(QStringLiteral(":sn"), inv.shop.name);
    q.bindValue(QStringLiteral(":sa"), inv.shop.address);
    q.bindValue(QStringLiteral(":sp"), sql::text(inv.shop.phone));
    q.bindValue(QStringLiteral(":sntn"), sql::text(inv.shop.ntn));
    q.bindValue(QStringLiteral(":sstrn"), sql::text(inv.shop.strn));
    q.bindValue(QStringLiteral(":bn"), sql::text(inv.buyerName));
    q.bindValue(QStringLiteral(":bp"), sql::text(inv.buyerPhone));
    q.bindValue(QStringLiteral(":ba"), sql::text(inv.buyerAddress));
    q.bindValue(QStringLiteral(":bc"), sql::text(inv.buyerCity));
    q.bindValue(QStringLiteral(":btn"), sql::text(inv.buyerTaxNumber));
    q.bindValue(QStringLiteral(":pay"), sql::text(inv.paymentText));
    q.bindValue(QStringLiteral(":why"), sql::text(inv.reason));
    q.bindValue(QStringLiteral(":goods"), inv.goods.paisa());
    q.bindValue(QStringLiteral(":ship"), inv.shipping.paisa());
    q.bindValue(QStringLiteral(":rate"), inv.taxRateBp);
    q.bindValue(QStringLiteral(":tax"), inv.tax.paisa());
    q.bindValue(QStringLiteral(":total"), inv.total.paisa());
    q.bindValue(QStringLiteral(":code"), inv.code);
    q.bindValue(QStringLiteral(":ret"), sql::nullable(inv.returnId));
    if (auto st = sql::exec(q, "InvoiceRepository::insert"); !st)
        return st;
    const qint64 id = q.lastInsertId().toLongLong();

    for (const InvoiceLine &line : inv.lines) {
        QSqlQuery li(db());
        li.prepare(QStringLiteral(
            "INSERT INTO invoice_lines (invoice_id, line_no, sku, description, quantity, unit_price_paisa, "
            "  value_paisa, tax_paisa) VALUES (:i, :n, :sku, :d, :q, :p, :v, :t)"));
        li.bindValue(QStringLiteral(":i"), id);
        li.bindValue(QStringLiteral(":n"), line.lineNo);
        li.bindValue(QStringLiteral(":sku"), sql::text(line.sku));
        li.bindValue(QStringLiteral(":d"), line.description);
        li.bindValue(QStringLiteral(":q"), line.quantity);
        li.bindValue(QStringLiteral(":p"), line.unitPrice.paisa());
        li.bindValue(QStringLiteral(":v"), line.value.paisa());
        li.bindValue(QStringLiteral(":t"), line.tax.paisa());
        if (auto st = sql::exec(li, "InvoiceRepository::insert(line)"); !st)
            return st;
    }
    return id;
}

Result<QVector<Invoice>> InvoiceRepository::select(const QString &where, const QVariantMap &binds, bool withLines,
                                                   const QString &tail) const
{
    QSqlQuery q(db());
    q.prepare(kSelectInvoice + where + tail);
    for (auto it = binds.cbegin(); it != binds.cend(); ++it)
        q.bindValue(it.key(), it.value());
    if (auto st = sql::exec(q, "InvoiceRepository::select"); !st)
        return st;
    QVector<Invoice> out;
    while (q.next())
        out.push_back(readInvoice(q));

    if (withLines) {
        for (Invoice &inv : out) {
            QSqlQuery li(db());
            li.prepare(QStringLiteral(
                "SELECT line_no, sku, description, quantity, unit_price_paisa, value_paisa, tax_paisa "
                "FROM invoice_lines WHERE invoice_id = :i ORDER BY line_no"));
            li.bindValue(QStringLiteral(":i"), inv.id);
            if (auto st = sql::exec(li, "InvoiceRepository::select(lines)"); !st)
                return st;
            while (li.next()) {
                InvoiceLine line;
                line.lineNo = li.value(0).toInt();
                line.sku = li.value(1).toString();
                line.description = li.value(2).toString();
                line.quantity = li.value(3).toInt();
                line.unitPrice = Money::fromPaisa(li.value(4).toLongLong());
                line.value = Money::fromPaisa(li.value(5).toLongLong());
                line.tax = Money::fromPaisa(li.value(6).toLongLong());
                inv.lines.push_back(line);
            }
        }
    }
    return out;
}

Result<std::optional<Invoice>> InvoiceRepository::findById(qint64 id) const
{
    auto found = select(QStringLiteral("WHERE i.id = :id"), {{QStringLiteral(":id"), id}}, true);
    if (!found)
        return found.status();
    if (found.value().isEmpty())
        return std::optional<Invoice>{};
    return std::optional<Invoice>{found.value().first()};
}

Result<std::optional<Invoice>> InvoiceRepository::findByNumber(const QString &number) const
{
    auto found = select(QStringLiteral("WHERE i.number = :n"), {{QStringLiteral(":n"), number.trimmed().toUpper()}}, true);
    if (!found)
        return found.status();
    if (found.value().isEmpty())
        return std::optional<Invoice>{};
    return std::optional<Invoice>{found.value().first()};
}

Result<OrderInvoices> InvoiceRepository::forOrder(qint64 orderId) const
{
    auto found = select(QStringLiteral("WHERE i.order_id = :o"), {{QStringLiteral(":o"), orderId}}, true,
                        QStringLiteral(" ORDER BY i.id"));
    if (!found)
        return found.status();
    OrderInvoices out;
    for (const Invoice &inv : found.value()) {
        if (inv.isReturnCredit())
            out.returnCreditNotes.push_back(inv);
        else if (inv.isCreditNote())
            out.creditNote = inv;
        else
            out.invoice = inv;
    }
    return out;
}

Result<QVector<Invoice>> InvoiceRepository::list(const InvoiceQuery &query, const QDateTime &fromUtc,
                                                 const QDateTime &toUtc) const
{
    QStringList where;
    QVariantMap binds;
    if (query.show == InvoiceQuery::Show::Invoices)
        where << QStringLiteral("i.kind = 'invoice'");
    else if (query.show == InvoiceQuery::Show::CreditNotes)
        where << QStringLiteral("i.kind = 'credit_note'");
    if (query.userId > 0) {
        where << QStringLiteral("o.user_id = :user");
        binds.insert(QStringLiteral(":user"), query.userId);
    }
    if (fromUtc.isValid()) {
        where << QStringLiteral("i.issued_at >= :from");
        binds.insert(QStringLiteral(":from"), sql::toDb(fromUtc));
    }
    if (toUtc.isValid()) {
        where << QStringLiteral("i.issued_at < :to");
        binds.insert(QStringLiteral(":to"), sql::toDb(toUtc));
    }
    const QString text = query.search.trimmed();
    if (!text.isEmpty()) {
        where << QStringLiteral("(i.number LIKE :s1 ESCAPE '\\' OR o.order_number LIKE :s2 ESCAPE '\\' "
                                "OR i.buyer_name LIKE :s3 ESCAPE '\\')");
        const QString like = QLatin1Char('%') + validation::escapeLike(text) + QLatin1Char('%');
        binds.insert(QStringLiteral(":s1"), like);
        binds.insert(QStringLiteral(":s2"), like);
        binds.insert(QStringLiteral(":s3"), like);
    }
    binds.insert(QStringLiteral(":lim"), qBound(1, query.limit, 20000));
    return select(where.isEmpty() ? QString() : QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")), binds,
                  false, QStringLiteral(" ORDER BY i.issued_at DESC, i.id DESC LIMIT :lim"));
}

} // namespace shophub
