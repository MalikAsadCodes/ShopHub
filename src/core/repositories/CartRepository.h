#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

namespace shophub {

// Carts are stored per user, so they survive restarts.
class CartRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    Result<QVector<CartItem>> items(qint64 userId) const;
    Result<int> quantityOf(qint64 userId, qint64 productId) const;
    Result<int> totalQuantity(qint64 userId) const;
    Status setQuantity(qint64 userId, qint64 productId, int quantity); // insert or update
    Status remove(qint64 userId, qint64 productId);
    Status clear(qint64 userId);

private:
    Result<qint64> ensureCart(qint64 userId);
};

} // namespace shophub
