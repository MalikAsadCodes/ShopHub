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
        b.itemCount += item.quantity;
    }
    b.shipping = shippingFor(b.subtotal);
    b.tax = b.subtotal.percentOf(m_rules.taxRateBp); // tax on goods only, not on shipping
    b.total = b.subtotal + b.shipping + b.tax;
    return b;
}

} // namespace shophub
