#include "services/CartService.h"

#include "services/Access.h"

namespace shophub {

CartService::CartService(Repositories &repos, const PricingService &pricing)
    : m_repos(repos), m_pricing(pricing)
{
}

Result<QVector<CartItem>> CartService::items(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return pricedCart(m_repos, session.userId());
}

Result<QVector<CartItem>> CartService::pricedCart(const Repositories &repos, qint64 userId)
{
    auto items = repos.carts.items(userId);
    if (!items)
        return items;
    QVector<qint64> productIds;
    for (const CartItem &i : items.value())
        productIds << i.productId;
    auto tiers = repos.products.tiersForProducts(productIds);
    if (!tiers)
        return tiers.status();
    PricingService::applyTiers(items.value(), tiers.value());
    return items;
}

Result<PriceBreakdown> CartService::summary(const Session &session) const
{
    auto list = items(session);
    if (!list)
        return list.status();
    return m_pricing.price(list.value());
}

Result<int> CartService::itemCount(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return m_repos.carts.totalQuantity(session.userId());
}

Status CartService::add(const Session &session, qint64 productId, int quantity)
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    if (quantity < 1)
        return Status::fail(QStringLiteral("Quantity must be at least 1."));

    auto product = m_repos.products.findById(productId);
    if (!product)
        return product.status();
    if (!product.value() || !product.value()->isPurchasable())
        return Status::fail(QStringLiteral("This product is not available."));
    const Product &p = *product.value();
    if (p.stock <= 0)
        return Status::fail(QStringLiteral("%1 is out of stock.").arg(p.name));

    auto current = m_repos.carts.quantityOf(session.userId(), productId);
    if (!current)
        return current.status();
    const int wanted = current.value() + quantity;
    if (wanted > kMaxPerLine)
        return Status::fail(QStringLiteral("You can order at most %1 of one item.").arg(kMaxPerLine));
    if (wanted > p.stock) {
        if (current.value() > 0)
            return Status::fail(QStringLiteral("Only %1 in stock and you already have %2 in your cart.")
                                    .arg(p.stock).arg(current.value()));
        return Status::fail(QStringLiteral("Only %1 left in stock.").arg(p.stock));
    }
    return m_repos.carts.setQuantity(session.userId(), productId, wanted);
}

Status CartService::setQuantity(const Session &session, qint64 productId, int quantity)
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    if (quantity <= 0)
        return m_repos.carts.remove(session.userId(), productId);
    if (quantity > kMaxPerLine)
        return Status::fail(QStringLiteral("You can order at most %1 of one item.").arg(kMaxPerLine));

    auto product = m_repos.products.findById(productId);
    if (!product)
        return product.status();
    if (!product.value())
        return Status::fail(QStringLiteral("This product is not available."));
    if (quantity > product.value()->stock)
        return Status::fail(QStringLiteral("Only %1 left in stock.").arg(product.value()->stock));
    return m_repos.carts.setQuantity(session.userId(), productId, quantity);
}

Status CartService::remove(const Session &session, qint64 productId)
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return m_repos.carts.remove(session.userId(), productId);
}

Status CartService::clear(const Session &session)
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return m_repos.carts.clear(session.userId());
}

QStringList CartService::problems(const QVector<CartItem> &items)
{
    QStringList out;
    for (const CartItem &i : items) {
        if (!i.productActive)
            out << QStringLiteral("%1 is no longer available. Please remove it.").arg(i.name);
        else if (i.availableStock <= 0)
            out << QStringLiteral("%1 is out of stock.").arg(i.name);
        else if (i.quantity > i.availableStock)
            out << QStringLiteral("Only %1 of %2 left in stock (you have %3).").arg(i.availableStock).arg(i.name).arg(i.quantity);
    }
    return out;
}

} // namespace shophub
