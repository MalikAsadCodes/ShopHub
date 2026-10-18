#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

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
};

} // namespace shophub
