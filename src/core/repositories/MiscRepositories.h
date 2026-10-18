#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

namespace shophub {

// Records security-relevant and administrative actions.
class AuditRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    Status log(qint64 userId, const QString &action, const QString &entity, qint64 entityId,
               const QString &details = QString());
};

// Key/value application settings stored in the database.
class SettingsRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    Result<QString> get(const QString &key, const QString &defaultValue = QString()) const;
    Status set(const QString &key, const QString &value);
};

// One default shipping address per customer (v1).
class AddressRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;
    Result<std::optional<Address>> defaultFor(qint64 userId) const;
    Status saveDefault(const Address &address);
};

} // namespace shophub
