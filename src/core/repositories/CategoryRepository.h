#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "repositories/RepositoryBase.h"

namespace shophub {

class CategoryRepository : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    Result<QVector<Category>> list(bool includeInactive) const;
    Result<std::optional<Category>> findById(qint64 id) const;
    Result<qint64> insert(const QString &name);
    Status rename(qint64 id, const QString &name);
    Status setActive(qint64 id, bool active);
};

} // namespace shophub
