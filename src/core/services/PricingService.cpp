#include "services/PricingService.h"

namespace shophub {

Money PricingService::shippingFor(Money subtotal) const
{
    if (subtotal <= Money())
        return Money();
    if (subtotal >= m_rules.freeShippingThreshold)
        return Money();
    return m_rules.shippingFlat;
}

PriceBreakdown PricingService::price(const QVector<CartItem> &items) const
{
    PriceBreakdown b;
    for (const CartItem &item : items) {
        b.subtotal += item.lineTotal();
        b.savings += item.lineSavings();
        b.itemCount += item.quantity;
    }
    // Free delivery and tax use the discounted subtotal: what the customer actually pays.
    b.shipping = shippingFor(b.subtotal);
    b.tax = b.subtotal.percentOf(m_rules.taxRateBp); // tax on goods only, not on shipping
    b.total = b.subtotal + b.shipping + b.tax;
    return b;
}

void PricingService::applyTiers(QVector<CartItem> &items, const QHash<qint64, QVector<PriceTier>> &tiersByProduct)
{
    for (CartItem &item : items) {
        if (item.basePrice.isZero())
            item.basePrice = item.unitPrice; // a line built without a normal price
        const QVector<PriceTier> tiers = tiersByProduct.value(item.productId);

        const std::optional<PriceTier> tier = pricetier::tierFor(tiers, item.quantity);
        item.unitPrice = tier ? pricetier::unitPrice(item.basePrice, *tier) : item.basePrice;
        item.discountBp = tier ? tier->discountBp : 0;

        item.nextTier = pricetier::nextTier(tiers, item.quantity);
        item.nextTierUnitPrice = item.nextTier ? pricetier::unitPrice(item.basePrice, *item.nextTier) : Money();
    }
}

} // namespace shophub
