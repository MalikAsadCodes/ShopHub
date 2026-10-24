#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"
#include "services/PricingService.h"

namespace shophub {

class CartService
{
public:
    static constexpr int kMaxPerLine = 99;

    CartService(Repositories &repos, const PricingService &pricing);

    Result<QVector<CartItem>> items(const Session &session) const;
    Result<PriceBreakdown> summary(const Session &session) const;
    Result<int> itemCount(const Session &session) const;

    Status add(const Session &session, qint64 productId, int quantity = 1);
    Status setQuantity(const Session &session, qint64 productId, int quantity); // 0 removes
    Status remove(const Session &session, qint64 productId);
    Status clear(const Session &session);

    // Human-readable problems that would block checkout (inactive or out-of-stock lines).
    static QStringList problems(const QVector<CartItem> &items);

    // A user's cart with wholesale prices applied. The cart screen and checkout
    // both use this, so the customer always pays the price they were shown.
    // No session check: callers check the session first.
    static Result<QVector<CartItem>> pricedCart(const Repositories &repos, qint64 userId);

private:
    Repositories &m_repos;
    const PricingService &m_pricing;
};

} // namespace shophub
