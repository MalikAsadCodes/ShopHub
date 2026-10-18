#pragma once

#include "common/Result.h"
#include "database/Database.h"
#include "repositories/Repositories.h"
#include "security/PasswordHasher.h"
#include "services/AuthService.h"
#include "services/CartService.h"
#include "services/CatalogService.h"
#include "services/CheckoutService.h"
#include "services/DashboardService.h"
#include "services/ImageStore.h"
#include "services/InventoryService.h"
#include "services/OrderService.h"
#include "services/PricingService.h"

#include <memory>

namespace shophub {

struct AppConfig
{
    QString databasePath;   // ":memory:" for tests
    QString imageDirectory;
    int passwordIterations = PasswordHasher::kDefaultIterations;
};

// Builds and owns the whole back end: database, repositories and services.
// The GUI receives a reference to this and talks only to the services.
class AppContext
{
public:
    static Result<std::unique_ptr<AppContext>> create(const AppConfig &config);
    ~AppContext();

    AppContext(const AppContext &) = delete;
    AppContext &operator=(const AppContext &) = delete;

    AuthService &auth() { return *m_auth; }
    CatalogService &catalog() { return *m_catalog; }
    CartService &cart() { return *m_cart; }
    CheckoutService &checkout() { return *m_checkout; }
    OrderService &orders() { return *m_orders; }
    InventoryService &inventory() { return *m_inventory; }
    DashboardService &dashboard() { return *m_dashboard; }
    const PricingService &pricing() const { return *m_pricing; }
    const ImageStore &images() const { return *m_images; }
    SettingsRepository &settings() { return m_repos->settings; }

    const QString &connectionName() const { return m_db->connectionName(); }

private:
    AppContext() = default;
    Status loadPricingRules();

    // Declaration order matters: members are destroyed bottom-up, so the
    // services go first and the database connection is closed last.
    std::unique_ptr<Database> m_db;
    std::unique_ptr<Repositories> m_repos;
    std::unique_ptr<PasswordHasher> m_hasher;
    std::unique_ptr<ImageStore> m_images;
    std::unique_ptr<PricingService> m_pricing;
    std::unique_ptr<AuthService> m_auth;
    std::unique_ptr<CatalogService> m_catalog;
    std::unique_ptr<CartService> m_cart;
    std::unique_ptr<CheckoutService> m_checkout;
    std::unique_ptr<OrderService> m_orders;
    std::unique_ptr<InventoryService> m_inventory;
    std::unique_ptr<DashboardService> m_dashboard;
};

} // namespace shophub
