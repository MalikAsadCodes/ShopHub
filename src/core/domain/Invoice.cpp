#include "domain/Invoice.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <numeric>

namespace shophub {

QString invoiceKindToDb(InvoiceKind kind)
{
    return kind == InvoiceKind::CreditNote ? QStringLiteral("credit_note") : QStringLiteral("invoice");
}

std::optional<InvoiceKind> invoiceKindFromDb(const QString &s)
{
    if (s == QLatin1String("invoice"))
        return InvoiceKind::Invoice;
    if (s == QLatin1String("credit_note"))
        return InvoiceKind::CreditNote;
    return std::nullopt;
}

namespace invoice {

namespace {

const char *const kOnes[] = {"Zero",    "One",     "Two",       "Three",    "Four",     "Five",    "Six",
                             "Seven",   "Eight",   "Nine",      "Ten",      "Eleven",   "Twelve",  "Thirteen",
                             "Fourteen", "Fifteen", "Sixteen",  "Seventeen", "Eighteen", "Nineteen"};
const char *const kTens[] = {"", "", "Twenty", "Thirty", "Forty", "Fifty", "Sixty", "Seventy", "Eighty", "Ninety"};

// 1 .. 999
QString wordsBelowThousand(int n)
{
    QStringList parts;
    if (n >= 100) {
        parts << QString::fromLatin1(kOnes[n / 100]) << QStringLiteral("Hundred");
        n %= 100;
    }
    if (n >= 20) {
        parts << QString::fromLatin1(kTens[n / 10]);
        n %= 10;
    }
    if (n > 0)
        parts << QString::fromLatin1(kOnes[n]);
    return parts.join(QLatin1Char(' '));
}

// 0 .. any: crore (10,000,000), lakh (100,000), thousand, then the rest.
QString wholeNumberInWords(qint64 n)
{
    if (n == 0)
        return QString::fromLatin1(kOnes[0]);
    QStringList parts;
    if (n >= 10'000'000) {
        parts << wholeNumberInWords(n / 10'000'000) << QStringLiteral("Crore");
        n %= 10'000'000;
    }
    if (n >= 100'000) {
        parts << wordsBelowThousand(int(n / 100'000)) << QStringLiteral("Lakh");
        n %= 100'000;
    }
    if (n >= 1000) {
        parts << wordsBelowThousand(int(n / 1000)) << QStringLiteral("Thousand");
        n %= 1000;
    }
    if (n > 0)
        parts << wordsBelowThousand(int(n));
    return parts.join(QLatin1Char(' '));
}

// Crockford's base 32: digits and letters without I, L, O and U.
const char kBase32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

} // namespace

QString formatNumber(InvoiceKind kind, int year, int seq)
{
    return QStringLiteral("%1-%2-%3")
        .arg(kind == InvoiceKind::CreditNote ? QStringLiteral("CN") : QStringLiteral("INV"))
        .arg(year, 4, 10, QLatin1Char('0'))
        .arg(seq, 6, 10, QLatin1Char('0'));
}

InvoiceKind kindOfNumber(const QString &number)
{
    return number.trimmed().startsWith(QLatin1String("CN-"), Qt::CaseInsensitive) ? InvoiceKind::CreditNote
                                                                                  : InvoiceKind::Invoice;
}

QVector<Money> allocateTax(const QVector<Money> &values, Money tax)
{
    QVector<Money> out(values.size());
    qint64 totalValue = 0;
    for (Money v : values)
        totalValue += qMax<qint64>(0, v.paisa());
    if (values.isEmpty() || totalValue <= 0 || tax.paisa() <= 0)
        return out;

    QVector<qint64> remainders(values.size());
    qint64 given = 0;
    for (int i = 0; i < values.size(); ++i) {
        const qint64 share = tax.paisa() * qMax<qint64>(0, values[i].paisa());
        out[i] = Money::fromPaisa(share / totalValue);
        remainders[i] = share % totalValue;
        given += share / totalValue;
    }
    QVector<int> order(values.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&remainders](int a, int b) { return remainders[a] > remainders[b]; });
    for (qint64 left = tax.paisa() - given, k = 0; left > 0; --left, ++k)
        out[order[int(k % order.size())]] += Money::fromPaisa(1);
    return out;
}

int deriveRateBp(Money goods, Money tax)
{
    if (goods.paisa() <= 0 || tax.paisa() <= 0)
        return 0;
    const qint64 bp = (tax.paisa() * 10000 * 2 + goods.paisa()) / (2 * goods.paisa());
    return int(qBound<qint64>(qint64(0), bp, qint64(10000)));
}

QString amountInWords(Money amount)
{
    const qint64 paisa = amount.paisa() < 0 ? -amount.paisa() : amount.paisa();
    QString text = QStringLiteral("Rupees ") + wholeNumberInWords(paisa / 100);
    if (paisa % 100 != 0)
        text += QStringLiteral(" and ") + wordsBelowThousand(int(paisa % 100)) + QStringLiteral(" Paisa");
    text += QStringLiteral(" Only");
    if (amount.isNegative())
        text.prepend(QStringLiteral("Minus "));
    return text;
}

bool isValidTaxNumber(const QString &text)
{
    static const QRegularExpression allowed(QStringLiteral("^[0-9]([0-9 \\-]*[0-9])?$"));
    const QString t = text.trimmed();
    if (t.size() > 24 || !allowed.match(t).hasMatch())
        return false;
    int digits = 0;
    for (const QChar c : t)
        digits += c.isDigit() ? 1 : 0;
    return digits >= 7 && digits <= 17;
}

QString normalizeTaxNumber(const QString &text)
{
    return text.simplified();
}

QString signatureCode(const QByteArray &key, InvoiceKind kind, const QString &number, Money total)
{
    const QByteArray message = QStringLiteral("ShopHub invoice v1|%1|%2|%3")
                                   .arg(invoiceKindToDb(kind), number.trimmed().toUpper())
                                   .arg(total.paisa())
                                   .toUtf8();
    const QByteArray mac = QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);

    // The first 10 bytes (80 bits) as 16 base-32 characters.
    QString code;
    quint64 buffer = 0;
    int bits = 0;
    for (int i = 0; i < 10; ++i) {
        buffer = (buffer << 8) | quint8(mac.at(i));
        bits += 8;
        while (bits >= 5) {
            code.append(QLatin1Char(kBase32[(buffer >> (bits - 5)) & 0x1F]));
            bits -= 5;
        }
    }
    return QStringLiteral("%1-%2-%3-%4").arg(code.mid(0, 4), code.mid(4, 4), code.mid(8, 4), code.mid(12, 4));
}

QString normalizeCode(const QString &code)
{
    QString out;
    for (QChar c : code.toUpper()) {
        if (c == QLatin1Char('-') || c.isSpace())
            continue;
        if (c == QLatin1Char('O'))
            c = QLatin1Char('0');
        else if (c == QLatin1Char('I') || c == QLatin1Char('L'))
            c = QLatin1Char('1');
        if (c.unicode() > 127 || !std::strchr(kBase32, char(c.unicode())))
            return QString();
        out.append(c);
    }
    return out.size() == 16 ? out : QString();
}

bool sameCode(const QString &a, const QString &b)
{
    const QString x = normalizeCode(a);
    const QString y = normalizeCode(b);
    if (x.isEmpty() || y.isEmpty() || x.size() != y.size())
        return false;
    int diff = 0; // compare every character, so the time taken says nothing
    for (int i = 0; i < x.size(); ++i)
        diff |= x.at(i).unicode() ^ y.at(i).unicode();
    return diff == 0;
}

QString qrText(const Invoice &inv)
{
    return QStringLiteral("%1|%2|%3|%4").arg(kQrPrefix, inv.number, inv.total.toPlainString(), inv.code);
}

std::optional<QrFields> parseQrText(const QString &text)
{
    const QStringList parts = text.trimmed().split(QLatin1Char('|'));
    if (parts.size() != 4 || parts.at(0).trimmed().compare(kQrPrefix, Qt::CaseInsensitive) != 0)
        return std::nullopt;
    const std::optional<Money> total = Money::parse(parts.at(2));
    const QString number = parts.at(1).trimmed().toUpper();
    if (!total || number.isEmpty() || normalizeCode(parts.at(3)).isEmpty())
        return std::nullopt;
    return QrFields{number, *total, parts.at(3).trimmed()};
}

} // namespace invoice
} // namespace shophub
