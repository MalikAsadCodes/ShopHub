#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"

namespace shophub {

// Admin dashboard figures, always computed from stored records.
// Gross sales = order totals (goods + shipping + tax) of all orders that
// are not cancelled. Cash on Delivery orders count when placed, not when paid.
class DashboardService
{
public:
    explicit DashboardService(Repositories &repos);
    Result<DashboardStats> stats(const Session &session) const;

private:
    Repositories &m_repos;
};

} // namespace shophub
