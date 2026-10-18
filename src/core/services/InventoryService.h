#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

class InventoryService
{
public:
    static constexpr int kMaxAdjustment = 100000;

    explicit InventoryService(Repositories &repos);

    // Admin only. Restock must add stock, Damaged must remove stock,
    // Adjustment can do either but needs a note. Returns the new stock level.
    Result<int> adjust(const Session &session, qint64 productId, int delta, StockReason reason, const QString &note);

    Result<QVector<InventoryMovement>> movements(const Session &session, qint64 productId = 0, int limit = 200) const;

private:
    Repositories &m_repos;
};

} // namespace shophub
