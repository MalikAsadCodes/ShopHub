#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"
#include "security/PasswordHasher.h"

namespace shophub {

class AuthService
{
public:
    static constexpr int kMaxFailedLogins = 5;
    static constexpr int kLockMinutes = 15;
    // PBKDF2 iterations for sample accounts, so 50 of them are created in about a second.
    static constexpr int kSampleIterations = 10000;

    AuthService(Repositories &repos, const PasswordHasher &hasher);

    Result<Session> login(const QString &email, const QString &password);
    Result<Session> registerCustomer(const QString &fullName, const QString &email, const QString &password);

    // Only allowed while the database has no administrator (first run).
    Result<Session> createFirstAdmin(const QString &fullName, const QString &email, const QString &password);

    Status changePassword(const Session &session, const QString &currentPassword, const QString &newPassword);
    void logout(const Session &session);

    Result<bool> hasAdmin() const;

    // Sample data only (DemoSeeder), admin only. Creates a customer whose password
    // skips the strength rule (sample passwords are "<FirstName>123") and is hashed
    // with kSampleIterations. The hash is upgraded to full strength automatically
    // the first time that customer signs in.
    Result<Session> createSampleCustomer(const Session &admin, const QString &fullName,
                                         const QString &email, const QString &password);

private:
    Result<Session> createUser(const QString &fullName, const QString &email, const QString &password, Role role,
                               const PasswordHasher &hasher, bool enforcePasswordRules);

    Repositories &m_repos;
    const PasswordHasher &m_hasher;
};

} // namespace shophub
