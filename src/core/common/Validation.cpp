#include "common/Validation.h"

#include <QRegularExpression>

namespace shophub::validation {

QString normalizeEmail(const QString &email)
{
    return email.trimmed().toLower();
}

bool isValidEmail(const QString &email)
{
    static const QRegularExpression re(
        QStringLiteral("^[A-Za-z0-9._%+\\-]+@[A-Za-z0-9\\-]+(\\.[A-Za-z0-9\\-]+)*\\.[A-Za-z]{2,}$"));
    const QString e = email.trimmed();
    return e.size() <= 254 && re.match(e).hasMatch();
}

Status checkPassword(const QString &password)
{
    if (password.size() < 8)
        return Status::fail(QStringLiteral("Password must be at least 8 characters long."));
    if (password.size() > 128)
        return Status::fail(QStringLiteral("Password must be at most 128 characters long."));

    bool hasLetter = false;
    bool hasDigit = false;
    for (const QChar c : password) {
        hasLetter = hasLetter || c.isLetter();
        hasDigit = hasDigit || c.isDigit();
    }
    if (!hasLetter || !hasDigit)
        return Status::fail(QStringLiteral("Password must contain at least one letter and one number."));
    return Status::ok();
}

Status checkRequired(const QString &value, const QString &fieldName, int maxLength)
{
    const QString v = value.trimmed();
    if (v.isEmpty())
        return Status::fail(QStringLiteral("%1 is required.").arg(fieldName));
    if (v.size() > maxLength)
        return Status::fail(QStringLiteral("%1 must be at most %2 characters.").arg(fieldName).arg(maxLength));
    return Status::ok();
}

bool isValidPhone(const QString &phone)
{
    static const QRegularExpression allowed(QStringLiteral("^\\+?[0-9 \\-]+$"));
    const QString p = phone.trimmed();
    if (!allowed.match(p).hasMatch())
        return false;
    int digits = 0;
    for (const QChar c : p)
        digits += c.isDigit() ? 1 : 0;
    return digits >= 10 && digits <= 15;
}

bool isValidSku(const QString &sku)
{
    static const QRegularExpression re(QStringLiteral("^[A-Z0-9][A-Z0-9\\-]{2,31}$"));
    return re.match(sku).hasMatch();
}

QString escapeLike(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c == QLatin1Char('\\') || c == QLatin1Char('%') || c == QLatin1Char('_'))
            out.append(QLatin1Char('\\'));
        out.append(c);
    }
    return out;
}

} // namespace shophub::validation
