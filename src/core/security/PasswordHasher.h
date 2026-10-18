#pragma once

#include <QString>

namespace shophub {

// PBKDF2-HMAC-SHA512 password hashing with a random 16-byte salt per password.
// Stored format:  pbkdf2_sha512$<iterations>$<salt base64>$<hash base64>
//
// The iteration count is stored with each hash, so it can be raised later
// and old hashes are upgraded automatically on the next successful login.
class PasswordHasher
{
public:
    static constexpr int kDefaultIterations = 210000; // OWASP recommendation for PBKDF2-SHA512

    explicit PasswordHasher(int iterations = kDefaultIterations);

    QString hash(const QString &password) const;
    bool verify(const QString &password, const QString &encoded) const;
    bool needsRehash(const QString &encoded) const;
    int iterations() const { return m_iterations; }

private:
    int m_iterations;
};

} // namespace shophub
