#include "services/DashboardService.h"

#include "services/Access.h"

#include <QMap>

namespace shophub {

DashboardService::DashboardService(Repositories &repos)
    : m_repos(repos)
{
}

Result<DashboardStats> DashboardService::stats(const Session &session) const
{
    if (auto st = access::requireAdmin(session); !st)
        return st;

    DashboardStats s;

    auto totals = m_repos.orders.salesTotals();
    if (!totals)
        return totals.status();
    s.grossSales = totals.value().first;
    s.orderCount = totals.value().second;
    s.averageOrderValue = s.orderCount > 0 ? Money::fromPaisa(s.grossSales.paisa() / s.orderCount) : Money();

    auto open = m_repos.orders.countWithStatus({OrderStatus::Pending, OrderStatus::Confirmed, OrderStatus::Processing});
    if (!open)
        return open.status();
    s.pendingOrders = open.value();

    auto low = m_repos.products.lowStockCount();
    if (!low)
        return low.status();
    s.lowStockCount = low.value();

    auto customers = m_repos.users.countByRole(Role::Customer);
    if (!customers)
        return customers.status();
    s.customerCount = customers.value();

    auto top = m_repos.orders.topProducts(5);
    if (!top)
        return top.status();
    s.topProducts = top.value();

    // Last 14 days, bucketed by the user's local date.
    const QDate today = QDate::currentDate();
    const QDate first = today.addDays(-13);
    auto sales = m_repos.orders.salesSince(first.startOfDay().toUTC());
    if (!sales)
        return sales.status();
    QMap<QDate, DailySales> byDay;
    for (QDate d = first; d <= today; d = d.addDays(1))
        byDay.insert(d, DailySales{d, Money(), 0});
    for (const auto &row : sales.value()) {
        const QDate d = row.first.toLocalTime().date();
        auto it = byDay.find(d);
        if (it != byDay.end()) {
            it->revenue += row.second;
            it->orders += 1;
        }
    }
    s.last14Days = byDay.values();

    OrderQuery recent;
    recent.limit = 8;
    auto orders = m_repos.orders.list(recent);
    if (!orders)
        return orders.status();
    s.recentOrders = orders.value();
    return s;
}

} // namespace shophub
