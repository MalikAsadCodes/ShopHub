#pragma once

#include "domain/Types.h"

namespace shophub {

struct PricingRules
{
    Money shippingFlat = Money::fromRupees(250);
    Money freeShippingThreshold = Money::fromRupees(5000); // subtotal at or above ships free
    int taxRateBp = 0;                                     // basis points, 1700 = 17%
};

// The single place where order totals are calculated. The GUI only
// displays these numbers; it never computes or submits totals itself.
class PricingService
{
public:
    PricingService() = default;
    explicit PricingService(PricingRules rules) : m_rules(rules) {}

    void setRules(const PricingRules &rules) { m_rules = rules; }
    const PricingRules &rules() const { return m_rules; }

    PriceBreakdown price(const QVector<CartItem> &items) const;
    Money shippingFor(Money subtotal) const;

private:
    PricingRules m_rules;
};

} // namespace shophub
