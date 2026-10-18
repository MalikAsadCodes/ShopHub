#include "security/PasswordHasher.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>

namespace shophub {

namespace {

const QString kScheme = QStringLiteral("pbkdf2_sha512");
constexpr int kSaltBytes = 16;
constexpr int kKeyBytes = 64;
constexpr int kMaxIterations = 10'000'000;

QByteArray derive(const QString &password, const QByteArray &salt, int iterations)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha512, password.toUtf8(), salt,
                                              iterations, kKeyBytes);
}

// Compares in constant time so the comparison leaks nothing about the hash.
bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

struct Parsed
{
    int iterations = 0;
    QByteArray salt;
    QByteArray key;
};

bool parse(const QString &encoded, Parsed &out)
{
    const QStringList parts = encoded.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts[0] != kScheme)
        return false;
    bool ok = false;
    out.iterations = parts[1].toInt(&ok);
    if (!ok || out.iterations < 1 || out.iterations > kMaxIterations)
        return false;
    out.salt = QByteArray::fromBase64(parts[2].toLatin1());
    out.key = QByteArray::fromBase64(parts[3].toLatin1());
    return out.salt.size() == kSaltBytes && out.key.size() == kKeyBytes;
}

} // namespace

PasswordHasher::PasswordHasher(int iterations)
    : m_iterations(qBound(1, iterations, kMaxIterations))
{
}

QString PasswordHasher::hash(const QString &password) const
{
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), kSaltBytes / 4);

    const QByteArray key = derive(password, salt, m_iterations);
    return QStringLiteral("%1$%2$%3$%4")
        .arg(kScheme)
        .arg(m_iterations)
        .arg(QString::fromLatin1(salt.toBase64()), QString::fromLatin1(key.toBase64()));
}

bool PasswordHasher::verify(const QString &password, const QString &encoded) const
{
    Parsed p;
    if (!parse(encoded, p))
        return false;
    return constantTimeEquals(derive(password, p.salt, p.iterations), p.key);
}

bool PasswordHasher::needsRehash(const QString &encoded) const
{
    Parsed p;
    return !parse(encoded, p) || p.iterations < m_iterations;
}

} // namespace shophub
