#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

#include <optional>

namespace shophub {

class UserRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    Result<std::optional<UserRecord>> findByEmail(const QString &email) const;
    Result<std::optional<User>> findById(qint64 id) const;
    Result<qint64> insert(const QString &email, const QString &fullName,
                          const QString &passwordHash, Role role);
    Status recordFailedLogin(qint64 id, int failedLogins, const QDateTime &lockedUntil);
    Status recordSuccessfulLogin(qint64 id);
    Status updatePasswordHash(qint64 id, const QString &passwordHash);
    Result<int> countByRole(Role role) const;
};

} // namespace shophub
