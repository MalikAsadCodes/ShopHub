#pragma once

#include "common/Result.h"
#include "domain/PriceTier.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

#include <QHash>

namespace shophub {

class ProductRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    Result<ProductPage> search(const ProductFilter &filter) const;
    Result<std::optional<Product>> findById(qint64 id) const;
    Result<qint64> insert(const ProductInput &input, int initialStock);
    Status update(qint64 id, const ProductInput &input);
    Status setActive(qint64 id, bool active);
    Result<bool> skuExists(const QString &sku, qint64 excludeId) const;
    Result<QStringList> brands() const;
    Result<int> lowStockCount() const;

    // Adds delta to stock only if the result stays >= 0 (atomic in SQL).
    // Returns the new stock level, or a failure if there was not enough stock.
    Result<int> changeStock(qint64 id, int delta);

    Status insertMovement(qint64 productId, int change, int stockAfter, StockReason reason,
                          const QString &note, qint64 userId);
    // productId == 0 returns movements for all products.
    Result<QVector<InventoryMovement>> movements(qint64 productId, int limit) const;

    // Wholesale (quantity) prices of one product, sorted by starting quantity.
    // Empty when the product has none.
    Result<QVector<PriceTier>> tiersFor(qint64 productId) const;

    // Tiers of several products at once (for pricing a cart), keyed by product
    // id and sorted by starting quantity. Products without tiers are missing.
    Result<QHash<qint64, QVector<PriceTier>>> tiersForProducts(const QVector<qint64> &productIds) const;

    // Replaces all tiers of a product; an empty list removes them. All or
    // nothing: it uses a savepoint, so a failed insert brings the old tiers
    // back even without an outer Transaction. Services still save inside a
    // Transaction, together with the audit log entry. The rules
    // (pricetier::validate) are checked by the service before calling this,
    // and the database enforces them too (migrations 3 and 4).
    Status replaceTiers(qint64 productId, const QVector<PriceTier> &tiers);
};

} // namespace shophub
