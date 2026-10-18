#pragma once

#include <QString>
#include <QtGlobal>
#include <optional>

namespace shophub {

// Exact money type. Amounts are stored as whole paisa (1 PKR = 100 paisa),
// so totals never suffer from floating-point rounding errors.
class Money
{
public:
    constexpr Money() = default;

    static constexpr Money fromPaisa(qint64 paisa) { Money m; m.m_paisa = paisa; return m; }
    static constexpr Money fromRupees(qint64 rupees) { return fromPaisa(rupees * 100); }

    // Parses user input such as "1500", "1,500.5" or "Rs 1500.50".
    // Returns nullopt for negative numbers, more than 2 decimals, or garbage.
    static std::optional<Money> parse(const QString &text);

    constexpr qint64 paisa() const { return m_paisa; }
    constexpr bool isZero() const { return m_paisa == 0; }
    constexpr bool isNegative() const { return m_paisa < 0; }

    // "Rs 1,234.50"
    QString toString() const;
    // "1234.50" - suitable for an edit field
    QString toPlainString() const;

    // Percentage expressed in basis points (1% = 100 bp), rounded half-up.
    Money percentOf(int basisPoints) const;

    constexpr Money operator+(Money o) const { return fromPaisa(m_paisa + o.m_paisa); }
    constexpr Money operator-(Money o) const { return fromPaisa(m_paisa - o.m_paisa); }
    constexpr Money operator*(qint64 qty) const { return fromPaisa(m_paisa * qty); }
    Money &operator+=(Money o) { m_paisa += o.m_paisa; return *this; }
    Money &operator-=(Money o) { m_paisa -= o.m_paisa; return *this; }

    constexpr bool operator==(Money o) const { return m_paisa == o.m_paisa; }
    constexpr bool operator!=(Money o) const { return m_paisa != o.m_paisa; }
    constexpr bool operator<(Money o) const { return m_paisa < o.m_paisa; }
    constexpr bool operator<=(Money o) const { return m_paisa <= o.m_paisa; }
    constexpr bool operator>(Money o) const { return m_paisa > o.m_paisa; }
    constexpr bool operator>=(Money o) const { return m_paisa >= o.m_paisa; }

private:
    qint64 m_paisa = 0;
};

} // namespace shophub
