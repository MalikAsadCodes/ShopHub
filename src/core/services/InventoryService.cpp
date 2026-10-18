#include "services/InventoryService.h"

#include "database/Transaction.h"
#include "services/Access.h"

namespace shophub {

InventoryService::InventoryService(Repositories &repos)
    : m_repos(repos)
{
}

Result<int> InventoryService::adjust(const Session &session, qint64 productId, int delta, StockReason reason,
                                     const QString &note)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (delta == 0)
        return Result<int>::fail(QStringLiteral("The quantity change cannot be zero."));
    if (delta > kMaxAdjustment || delta < -kMaxAdjustment)
        return Result<int>::fail(QStringLiteral("You can change stock by at most %1 at a time.").arg(kMaxAdjustment));

    switch (reason) {
    case StockReason::Restock:
        if (delta < 0)
            return Result<int>::fail(QStringLiteral("A restock must add stock."));
        break;
    case StockReason::Damaged:
        if (delta > 0)
            return Result<int>::fail(QStringLiteral("Damaged or lost stock must be a negative change."));
        break;
    case StockReason::Adjustment:
        if (note.trimmed().isEmpty())
            return Result<int>::fail(QStringLiteral("Please add a note explaining the adjustment."));
        break;
    default:
        return Result<int>::fail(QStringLiteral("This kind of stock change is recorded automatically."));
    }

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    auto product = m_repos.products.findById(productId);
    if (!product)
        return product.status();
    if (!product.value())
        return Result<int>::fail(QStringLiteral("Product not found."));
    if (product.value()->stock + delta < 0)
        return Result<int>::fail(QStringLiteral("Stock cannot go below zero (current stock is %1).")
                                     .arg(product.value()->stock));

    auto newStock = m_repos.products.changeStock(productId, delta);
    if (!newStock)
        return newStock;
    if (auto st = m_repos.products.insertMovement(productId, delta, newStock.value(), reason, note.trimmed(),
                                                  session.userId());
        !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("stock_adjusted"), QStringLiteral("product"),
                                    productId,
                                    QStringLiteral("%1 %2%3").arg(stockReasonToDb(reason), delta > 0 ? QStringLiteral("+") : QString())
                                        .arg(delta));
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    return newStock;
}

Result<QVector<InventoryMovement>> InventoryService::movements(const Session &session, qint64 productId, int limit) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return m_repos.products.movements(productId, limit);
}

} // namespace shophub
