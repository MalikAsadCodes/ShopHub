#include "repositories/ReturnRepository.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace shophub {

namespace {

const QString kSelectReturn = QStringLiteral(
    "SELECT r.id, r.order_id, o.order_number, r.user_id, u.full_name, u.email, o.payment_method, "
    "       r.status, r.reason, r.note, r.decision_note, r.late, "
    "       COALESCE(rb.full_name, ''), COALESCE(db.full_name, ''), r.created_at, r.decided_at, r.updated_at, "
    "       (SELECT COALESCE(SUM(l.quantity), 0) FROM return_lines l WHERE l.return_id = r.id), "
    "       (SELECT COALESCE(SUM(l.quantity * oi.unit_price_paisa), 0) FROM return_lines l "
    "          JOIN order_items oi ON oi.id = l.order_item_id WHERE l.return_id = r.id), "
    "       rf.method, rf.reference, rf.goods_paisa, rf.tax_paisa, rf.shipping_paisa, rf.total_paisa, "
    "       rf.markup_paisa, rf.cash_back_paisa, rf.credit_note_id, COALESCE(cn.number, ''), rf.note, "
    "       COALESCE(rcv.full_name, ''), rf.received_at "
    "FROM returns r "
    "JOIN orders o ON o.id = r.order_id "
    "JOIN users u ON u.id = r.user_id "
    "LEFT JOIN users rb ON rb.id = r.requested_by "
    "LEFT JOIN users db ON db.id = r.decided_by "
    "LEFT JOIN return_refunds rf ON rf.return_id = r.id "
    "LEFT JOIN invoices cn ON cn.id = rf.credit_note_id "
    "LEFT JOIN users rcv ON rcv.id = rf.received_by ");

ReturnRequest readReturn(const QSqlQuery &q)
{
    ReturnRequest r;
    r.id = q.value(0).toLongLong();
    r.orderId = q.value(1).toLongLong();
    r.orderNumber = q.value(2).toString();
    r.userId = q.value(3).toLongLong();
    r.customerName = q.value(4).toString();
    r.customerEmail = q.value(5).toString();
    r.paymentMethod = q.value(6).toString();
    r.status = returns::statusFromDb(q.value(7).toString()).value_or(ReturnStatus::Requested);
    r.reason = returns::reasonFromDb(q.value(8).toString()).value_or(ReturnReason::Defective);
    r.note = q.value(9).toString();
    r.decisionNote = q.value(10).toString();
    r.late = q.value(11).toInt() != 0;
    r.requestedBy = q.value(12).toString();
    r.decidedBy = q.value(13).toString();
    r.createdAt = sql::fromDb(q.value(14));
    r.decidedAt = sql::fromDb(q.value(15));
    r.updatedAt = sql::fromDb(q.value(16));
    r.itemCount = q.value(17).toInt();
    r.value = Money::fromPaisa(q.value(18).toLongLong());
    if (!q.value(19).isNull()) {
        ReturnRefund f;
        f.method = returns::refundMethodFromDb(q.value(19).toString()).value_or(RefundMethod::Cash);
        f.reference = q.value(20).toString();
        f.goods = Money::fromPaisa(q.value(21).toLongLong());
        f.tax = Money::fromPaisa(q.value(22).toLongLong());
        f.shipping = Money::fromPaisa(q.value(23).toLongLong());
        f.total = Money::fromPaisa(q.value(24).toLongLong());
        f.markup = Money::fromPaisa(q.value(25).toLongLong());
        f.cashBack = Money::fromPaisa(q.value(26).toLongLong());
        f.creditNoteId = q.value(27).isNull() ? 0 : q.value(27).toLongLong();
        f.creditNoteNumber = q.value(28).toString();
        f.note = q.value(29).toString();
        f.receivedBy = q.value(30).toString();
        f.receivedAt = sql::fromDb(q.value(31));
        r.refund = f;
    }
    return r;
}

} // namespace

Result<qint64> ReturnRepository::insert(const ReturnRequest &request, qint64 requestedBy)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO returns (order_id, user_id, status, reason, note, decision_note, late, requested_by, "
        "  decided_by, created_at, decided_at, updated_at) "
        "VALUES (:o, :u, :s, :r, :n, :dn, :late, :rb, :db, :c, :d, :up)"));
    const QString now = sql::nowUtc();
    const bool decided = request.status != ReturnStatus::Requested; // opened by the shop: approved at once
    q.bindValue(QStringLiteral(":o"), request.orderId);
    q.bindValue(QStringLiteral(":u"), request.userId);
    q.bindValue(QStringLiteral(":s"), returns::statusToDb(request.status));
    q.bindValue(QStringLiteral(":r"), returns::reasonToDb(request.reason));
    q.bindValue(QStringLiteral(":n"), sql::text(request.note));
    q.bindValue(QStringLiteral(":dn"), sql::text(request.decisionNote));
    q.bindValue(QStringLiteral(":late"), request.late ? 1 : 0);
    q.bindValue(QStringLiteral(":rb"), sql::nullable(requestedBy));
    q.bindValue(QStringLiteral(":db"), decided ? sql::nullable(requestedBy) : QVariant(QMetaType(QMetaType::LongLong)));
    q.bindValue(QStringLiteral(":c"), now);
    q.bindValue(QStringLiteral(":d"), decided ? QVariant(now) : QVariant(QMetaType(QMetaType::QString)));
    q.bindValue(QStringLiteral(":up"), now);
    if (auto st = sql::exec(q, "ReturnRepository::insert"); !st)
        return st;
    const qint64 id = q.lastInsertId().toLongLong();

    for (const ReturnLine &line : request.lines) {
        QSqlQuery li(db());
        li.prepare(QStringLiteral(
            "INSERT INTO return_lines (return_id, order_item_id, quantity) VALUES (:r, :i, :q)"));
        li.bindValue(QStringLiteral(":r"), id);
        li.bindValue(QStringLiteral(":i"), line.orderItemId);
        li.bindValue(QStringLiteral(":q"), line.quantity);
        if (auto st = sql::exec(li, "ReturnRepository::insert(line)"); !st)
            return st;
    }
    return id;
}

Status ReturnRepository::loadLines(ReturnRequest &request) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT l.order_item_id, oi.product_id, oi.sku, oi.product_name, oi.unit_price_paisa, l.quantity, "
        "       COALESCE(rc.restocked, 0), COALESCE(rc.damaged, 0), COALESCE(rc.tax_paisa, 0) "
        "FROM return_lines l JOIN order_items oi ON oi.id = l.order_item_id "
        "LEFT JOIN return_receipts rc ON rc.return_id = l.return_id AND rc.order_item_id = l.order_item_id "
        "WHERE l.return_id = :r ORDER BY oi.id"));
    q.bindValue(QStringLiteral(":r"), request.id);
    if (auto st = sql::exec(q, "ReturnRepository::loadLines"); !st)
        return st;
    request.lines.clear();
    while (q.next()) {
        ReturnLine line;
        line.orderItemId = q.value(0).toLongLong();
        if (!q.value(1).isNull())
            line.productId = q.value(1).toLongLong();
        line.sku = q.value(2).toString();
        line.productName = q.value(3).toString();
        line.unitPrice = Money::fromPaisa(q.value(4).toLongLong());
        line.quantity = q.value(5).toInt();
        line.restocked = q.value(6).toInt();
        line.damaged = q.value(7).toInt();
        line.creditedTax = Money::fromPaisa(q.value(8).toLongLong());
        request.lines.push_back(line);
    }
    return Status::ok();
}

Result<QVector<ReturnRequest>> ReturnRepository::select(const QString &where, const QVariantMap &binds,
                                                        bool withLines, const QString &tail) const
{
    QSqlQuery q(db());
    q.prepare(kSelectReturn + where + tail);
    for (auto it = binds.cbegin(); it != binds.cend(); ++it)
        q.bindValue(it.key(), it.value());
    if (auto st = sql::exec(q, "ReturnRepository::select"); !st)
        return st;
    QVector<ReturnRequest> out;
    while (q.next())
        out.push_back(readReturn(q));
    if (withLines) {
        for (ReturnRequest &r : out) {
            if (auto st = loadLines(r); !st)
                return st;
        }
    }
    return out;
}

Result<std::optional<ReturnRequest>> ReturnRepository::findById(qint64 id) const
{
    auto rows = select(QStringLiteral("WHERE r.id = :id"), {{QStringLiteral(":id"), id}}, true, QString());
    if (!rows)
        return rows.status();
    if (rows.value().isEmpty())
        return std::optional<ReturnRequest>{};
    return std::optional<ReturnRequest>{rows.value().first()};
}

Result<QVector<ReturnRequest>> ReturnRepository::forOrder(qint64 orderId) const
{
    return select(QStringLiteral("WHERE r.order_id = :o"), {{QStringLiteral(":o"), orderId}}, true,
                  QStringLiteral(" ORDER BY r.created_at DESC, r.id DESC"));
}

Result<QVector<ReturnRequest>> ReturnRepository::list(const ReturnQuery &query) const
{
    QStringList where;
    QVariantMap binds;
    switch (query.show) {
    case ReturnQuery::Show::Open:
        where << QStringLiteral("r.status IN ('requested', 'approved', 'received')");
        break;
    case ReturnQuery::Show::Requested:
        where << QStringLiteral("r.status = 'requested'");
        break;
    case ReturnQuery::Show::Approved:
        where << QStringLiteral("r.status = 'approved'");
        break;
    case ReturnQuery::Show::Closed:
        where << QStringLiteral("r.status IN ('rejected', 'cancelled', 'completed')");
        break;
    case ReturnQuery::Show::All:
        break;
    }
    if (query.userId > 0) {
        where << QStringLiteral("r.user_id = :user");
        binds.insert(QStringLiteral(":user"), query.userId);
    }
    QString text = query.search.trimmed();
    if (!text.isEmpty()) {
        // "RET-000012" (or "ret-12") finds the return by its number.
        bool isNumber = false;
        const QString digits = text.startsWith(QLatin1String("RET-"), Qt::CaseInsensitive) ? text.mid(4) : QString();
        const qint64 returnId = digits.toLongLong(&isNumber);
        if (isNumber && returnId > 0) {
            where << QStringLiteral("r.id = :rid");
            binds.insert(QStringLiteral(":rid"), returnId);
        } else {
            where << QStringLiteral("(o.order_number LIKE :s1 ESCAPE '\\' OR u.full_name LIKE :s2 ESCAPE '\\' "
                                    "OR u.email LIKE :s3 ESCAPE '\\')");
            const QString like = QLatin1Char('%') + validation::escapeLike(text) + QLatin1Char('%');
            binds.insert(QStringLiteral(":s1"), like);
            binds.insert(QStringLiteral(":s2"), like);
            binds.insert(QStringLiteral(":s3"), like);
        }
    }
    // Waiting for the shop first (requested, then approved, then received), newest first within each.
    const QString order = QStringLiteral(
        " ORDER BY CASE r.status WHEN 'requested' THEN 0 WHEN 'approved' THEN 1 WHEN 'received' THEN 2 ELSE 3 END, "
        "r.created_at DESC, r.id DESC LIMIT 1000");
    return select(where.isEmpty() ? QString() : QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")), binds,
                  false, order);
}

Result<QHash<qint64, int>> ReturnRepository::returnedUnits(qint64 orderId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT l.order_item_id, SUM(l.quantity) FROM return_lines l JOIN returns r ON r.id = l.return_id "
        "WHERE r.order_id = :o AND r.status NOT IN ('rejected', 'cancelled') GROUP BY l.order_item_id"));
    q.bindValue(QStringLiteral(":o"), orderId);
    if (auto st = sql::exec(q, "ReturnRepository::returnedUnits"); !st)
        return st;
    QHash<qint64, int> out;
    while (q.next())
        out.insert(q.value(0).toLongLong(), q.value(1).toInt());
    return out;
}

Status ReturnRepository::setStatus(qint64 id, ReturnStatus status, qint64 decidedBy, const QString &decisionNote)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE returns SET status = :s, decided_by = :by, decision_note = :n, decided_at = :t, updated_at = :t2 "
        "WHERE id = :id"));
    const QString now = sql::nowUtc();
    q.bindValue(QStringLiteral(":s"), returns::statusToDb(status));
    q.bindValue(QStringLiteral(":by"), sql::nullable(decidedBy));
    q.bindValue(QStringLiteral(":n"), sql::text(decisionNote));
    q.bindValue(QStringLiteral(":t"), now);
    q.bindValue(QStringLiteral(":t2"), now);
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "ReturnRepository::setStatus");
}

Status ReturnRepository::setStatusOnly(qint64 id, ReturnStatus status)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE returns SET status = :s, updated_at = :t WHERE id = :id"));
    q.bindValue(QStringLiteral(":s"), returns::statusToDb(status));
    q.bindValue(QStringLiteral(":t"), sql::nowUtc());
    q.bindValue(QStringLiteral(":id"), id);
    return sql::exec(q, "ReturnRepository::setStatusOnly");
}

Status ReturnRepository::insertReceipt(qint64 returnId, const ReturnLine &line)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO return_receipts (return_id, order_item_id, restocked, damaged, value_paisa, tax_paisa) "
        "VALUES (:r, :i, :rs, :dm, :v, :t)"));
    q.bindValue(QStringLiteral(":r"), returnId);
    q.bindValue(QStringLiteral(":i"), line.orderItemId);
    q.bindValue(QStringLiteral(":rs"), line.restocked);
    q.bindValue(QStringLiteral(":dm"), line.damaged);
    q.bindValue(QStringLiteral(":v"), line.value().paisa());
    q.bindValue(QStringLiteral(":t"), line.creditedTax.paisa());
    return sql::exec(q, "ReturnRepository::insertReceipt");
}

Status ReturnRepository::insertRefund(qint64 returnId, const ReturnRefund &f, qint64 receivedBy)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO return_refunds (return_id, method, reference, goods_paisa, tax_paisa, shipping_paisa, total_paisa, "
        "  markup_paisa, cash_back_paisa, credit_note_id, note, received_by, received_at) "
        "VALUES (:r, :m, :ref, :g, :t, :s, :tot, :mk, :cb, :cn, :n, :by, :at)"));
    q.bindValue(QStringLiteral(":r"), returnId);
    q.bindValue(QStringLiteral(":m"), returns::refundMethodToDb(f.method));
    q.bindValue(QStringLiteral(":ref"), sql::text(f.reference));
    q.bindValue(QStringLiteral(":g"), f.goods.paisa());
    q.bindValue(QStringLiteral(":t"), f.tax.paisa());
    q.bindValue(QStringLiteral(":s"), f.shipping.paisa());
    q.bindValue(QStringLiteral(":tot"), f.total.paisa());
    q.bindValue(QStringLiteral(":mk"), f.markup.paisa());
    q.bindValue(QStringLiteral(":cb"), f.cashBack.paisa());
    q.bindValue(QStringLiteral(":cn"), sql::nullable(f.creditNoteId));
    q.bindValue(QStringLiteral(":n"), sql::text(f.note));
    q.bindValue(QStringLiteral(":by"), sql::nullable(receivedBy));
    q.bindValue(QStringLiteral(":at"), sql::nowUtc());
    return sql::exec(q, "ReturnRepository::insertRefund");
}

Result<QHash<qint64, ReturnRepository::Credited>> ReturnRepository::creditedLines(qint64 orderId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT rc.order_item_id, SUM(rc.restocked + rc.damaged), SUM(rc.tax_paisa) FROM return_receipts rc "
        "JOIN returns r ON r.id = rc.return_id WHERE r.order_id = :o GROUP BY rc.order_item_id"));
    q.bindValue(QStringLiteral(":o"), orderId);
    if (auto st = sql::exec(q, "ReturnRepository::creditedLines"); !st)
        return st;
    QHash<qint64, Credited> out;
    while (q.next())
        out.insert(q.value(0).toLongLong(), Credited{q.value(1).toInt(), Money::fromPaisa(q.value(2).toLongLong())});
    return out;
}

Result<QPair<Money, Money>> ReturnRepository::refundedForOrder(qint64 orderId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(rf.shipping_paisa), 0), "
        "       COALESCE(SUM(CASE WHEN rf.method = 'qist' THEN rf.markup_paisa ELSE 0 END), 0) "
        "FROM return_refunds rf JOIN returns r ON r.id = rf.return_id WHERE r.order_id = :o"));
    q.bindValue(QStringLiteral(":o"), orderId);
    if (auto st = sql::exec(q, "ReturnRepository::refundedForOrder"); !st)
        return st;
    q.next();
    return qMakePair(Money::fromPaisa(q.value(0).toLongLong()), Money::fromPaisa(q.value(1).toLongLong()));
}

Result<int> ReturnRepository::countWithStatus(ReturnStatus status) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM returns WHERE status = :s"));
    q.bindValue(QStringLiteral(":s"), returns::statusToDb(status));
    if (auto st = sql::exec(q, "ReturnRepository::countWithStatus"); !st)
        return st;
    q.next();
    return q.value(0).toInt();
}

} // namespace shophub
