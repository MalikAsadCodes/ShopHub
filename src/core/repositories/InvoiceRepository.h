#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

#include <QVariantMap>

namespace shophub {

// Invoices and credit notes with their lines. Rows are only ever inserted:
// the database refuses updates, deletes and numbers out of sequence
// (migration 6). See docs/INVOICES.md.
class InvoiceRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    // The number the next invoice (or credit note) of this kind must have.
    Result<int> nextSeq(InvoiceKind kind) const;

    // Inserts the header and its lines. issuedBy = the user issuing it.
    Result<qint64> insert(const Invoice &invoice, qint64 issuedBy);

    Result<std::optional<Invoice>> findById(qint64 id) const;               // with lines
    Result<std::optional<Invoice>> findByNumber(const QString &number) const; // with lines
    Result<OrderInvoices> forOrder(qint64 orderId) const;                   // with lines

    // Newest first, without lines. from/to (UTC) limit the issue time when valid.
    Result<QVector<Invoice>> list(const InvoiceQuery &query, const QDateTime &fromUtc, const QDateTime &toUtc) const;

private:
    Result<QVector<Invoice>> select(const QString &where, const QVariantMap &binds, bool withLines,
                                    const QString &tail = QString()) const;
};

} // namespace shophub
