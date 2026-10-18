#include "common/Money.h"

#include <QRegularExpression>

namespace shophub {

std::optional<Money> Money::parse(const QString &text)
{
    QString s = text.trimmed();
    if (s.startsWith(QLatin1String("Rs"), Qt::CaseInsensitive))
        s = s.mid(2).trimmed();
    s.remove(QLatin1Char(','));

    // Up to 13 integer digits keeps us far away from qint64 overflow.
    static const QRegularExpression re(QStringLiteral("^(\\d{1,13})(?:\\.(\\d{1,2}))?$"));
    const QRegularExpressionMatch m = re.match(s);
    if (!m.hasMatch())
        return std::nullopt;

    const qint64 whole = m.captured(1).toLongLong();
    QString frac = m.captured(2);
    while (frac.size() < 2)
        frac.append(QLatin1Char('0'));

    return fromPaisa(whole * 100 + frac.toLongLong());
}

QString Money::toPlainString() const
{
    const qint64 abs = m_paisa < 0 ? -m_paisa : m_paisa;
    return QStringLiteral("%1%2.%3")
        .arg(m_paisa < 0 ? QStringLiteral("-") : QString())
        .arg(abs / 100)
        .arg(abs % 100, 2, 10, QLatin1Char('0'));
}

QString Money::toString() const
{
    const qint64 abs = m_paisa < 0 ? -m_paisa : m_paisa;
    QString whole = QString::number(abs / 100);
    for (int i = whole.size() - 3; i > 0; i -= 3)
        whole.insert(i, QLatin1Char(','));

    return QStringLiteral("%1Rs %2.%3")
        .arg(m_paisa < 0 ? QStringLiteral("-") : QString())
        .arg(whole)
        .arg(abs % 100, 2, 10, QLatin1Char('0'));
}

Money Money::percentOf(int basisPoints) const
{
    // Round half away from zero.
    const qint64 product = m_paisa * basisPoints;
    const qint64 rounded = product >= 0 ? (product + 5000) / 10000 : (product - 5000) / 10000;
    return fromPaisa(rounded);
}

} // namespace shophub
