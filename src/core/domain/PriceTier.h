#pragma once

// Wholesale (quantity) prices - v2 milestone 2. See docs/WHOLESALE.md.
//
// A tier lowers the unit price of ONE product when a customer buys enough of
// that product in one order line, e.g. "12 or more: 5% off". These are the
// shop's SELLING tiers. What a supplier charges the shop (buying tiers) will
// be a separate type in the supplier module, so the two never mix.

#include "common/Money.h"

#include <QRegularExpression>
#include <QString>
#include <QVector>
#include <optional>

namespace shophub {

struct PriceTier
{
    int minQuantity = 0; // the tier applies from this quantity upward
    int discountBp = 0;  // discount in basis points: 500 = 5% off, 1000 = 10% off
};

namespace pricetier {

// A cart line holds 1-99 units, so a tier must start between 2 and 99.
constexpr int kMinQuantity = 2;
constexpr int kMaxQuantity = 99;
constexpr int kMaxDiscountBp = 9000; // at most 90% off
constexpr int kMaxTiersPerProduct = 5;

// The tier for a quantity: the one with the highest minQuantity that the
// quantity reaches. Empty when no tier applies (the normal price is charged).
inline std::optional<PriceTier> tierFor(const QVector<PriceTier> &tiers, int quantity)
{
    std::optional<PriceTier> best;
    for (const PriceTier &t : tiers) {
        if (quantity >= t.minQuantity && (!best || t.minQuantity > best->minQuantity))
            best = t;
    }
    return best;
}

// The next tier above a quantity, for hints like "add 3 more for Rs 950 each".
inline std::optional<PriceTier> nextTier(const QVector<PriceTier> &tiers, int quantity)
{
    std::optional<PriceTier> next;
    for (const PriceTier &t : tiers) {
        if (t.minQuantity > quantity && (!next || t.minQuantity < next->minQuantity))
            next = t;
    }
    return next;
}

// Unit price with a tier's discount. The discount is rounded half-up to whole
// paisa, the same way tax is, and the price never drops below 1 paisa.
inline Money unitPrice(Money basePrice, const PriceTier &tier)
{
    const Money discounted = basePrice - basePrice.percentOf(tier.discountBp);
    return discounted.paisa() < 1 ? Money::fromPaisa(1) : discounted;
}

// Unit price for buying `quantity` of a product with these tiers.
inline Money unitPriceFor(Money basePrice, const QVector<PriceTier> &tiers, int quantity)
{
    const std::optional<PriceTier> tier = tierFor(tiers, quantity);
    return tier ? unitPrice(basePrice, *tier) : basePrice;
}

// Checks a product's tiers as the admin entered them (in any order).
// Returns an empty string when they are fine, otherwise a message for the admin.
// Rules: at most 5 tiers, each starting at 2-99 units, a discount of 0.01%-90%,
// no two tiers starting at the same quantity, and a bigger quantity must never
// get a smaller discount.
inline QString validate(const QVector<PriceTier> &tiers)
{
    if (tiers.size() > kMaxTiersPerProduct)
        return QStringLiteral("A product can have at most %1 quantity prices.").arg(kMaxTiersPerProduct);

    for (const PriceTier &t : tiers) {
        if (t.minQuantity < kMinQuantity || t.minQuantity > kMaxQuantity)
            return QStringLiteral("A quantity price must start between %1 and %2 units.").arg(kMinQuantity).arg(kMaxQuantity);
        if (t.discountBp <= 0 || t.discountBp > kMaxDiscountBp)
            return QStringLiteral("The discount must be more than 0% and at most %1%.").arg(kMaxDiscountBp / 100);
    }

    for (const PriceTier &a : tiers) {
        for (const PriceTier &b : tiers) {
            if (&a == &b)
                continue;
            if (a.minQuantity == b.minQuantity)
                return QStringLiteral("Two quantity prices start at %1 units. Keep only one.").arg(a.minQuantity);
            if (a.minQuantity < b.minQuantity && a.discountBp > b.discountBp)
                return QStringLiteral("Buying %1 units must not cost more per unit than buying %2.")
                    .arg(b.minQuantity)
                    .arg(a.minQuantity);
        }
    }
    return QString();
}

// Basis points as a percentage for people to read: 500 -> "5", 250 -> "2.50".
// Integer maths only, so it never shows a rounding artefact.
inline QString percentText(int basisPoints)
{
    QString text = QString::number(basisPoints / 100);
    if (basisPoints % 100 != 0)
        text += QStringLiteral(".%1").arg(basisPoints % 100, 2, 10, QLatin1Char('0'));
    return text;
}

// What an admin types as a discount ("5", "2.5", "12.75%") to basis points.
// Empty when it is not a percentage with at most 2 decimals. The range
// (more than 0, at most 90) is checked by validate().
inline std::optional<int> parsePercent(const QString &input)
{
    QString text = input.trimmed();
    if (text.endsWith(QLatin1Char('%')))
        text.chop(1);
    static const QRegularExpression pattern(QStringLiteral("^(\\d{1,3})(?:\\.(\\d{1,2}))?$"));
    const QRegularExpressionMatch m = pattern.match(text.trimmed());
    if (!m.hasMatch())
        return std::nullopt;
    QString fraction = m.captured(2);
    while (fraction.size() < 2)
        fraction.append(QLatin1Char('0')); // "5" -> 50 basis points
    return m.captured(1).toInt() * 100 + fraction.toInt();
}

} // namespace pricetier
} // namespace shophub
