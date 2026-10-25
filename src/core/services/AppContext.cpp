#include "services/AppContext.h"

#include "database/Migrations.h"

#include <QtDebug>

namespace shophub {

Result<std::unique_ptr<AppContext>> AppContext::create(const AppConfig &config)
{
    std::unique_ptr<AppContext> ctx(new AppContext());

    ctx->m_db = std::make_unique<Database>();
    if (auto st = ctx->m_db->open(config.databasePath); !st)
        return st;

    Migrator migrator(ctx->m_db->connectionName());
    if (auto st = migrator.migrate(); !st)
        return st;

    ctx->m_repos = std::make_unique<Repositories>(ctx->m_db->connectionName());
    ctx->m_hasher = std::make_unique<PasswordHasher>(config.passwordIterations);
    ctx->m_images = std::make_unique<ImageStore>(config.imageDirectory);
    ctx->m_pricing = std::make_unique<PricingService>();
    if (auto st = ctx->loadPricingRules(); !st)
        return st;

    Repositories &r = *ctx->m_repos;
    ctx->m_auth = std::make_unique<AuthService>(r, *ctx->m_hasher);
    ctx->m_catalog = std::make_unique<CatalogService>(r, *ctx->m_images);
    ctx->m_cart = std::make_unique<CartService>(r, *ctx->m_pricing);
    ctx->m_checkout = std::make_unique<CheckoutService>(r, *ctx->m_pricing);
    ctx->m_orders = std::make_unique<OrderService>(r);
    ctx->m_inventory = std::make_unique<InventoryService>(r);
    ctx->m_dashboard = std::make_unique<DashboardService>(r);
    ctx->m_khata = std::make_unique<KhataService>(r);
    ctx->m_qist = std::make_unique<QistService>(r);

    qInfo() << "ShopHub back end ready, schema version" << migrator.currentVersion();
    return Result<std::unique_ptr<AppContext>>(std::move(ctx));
}

AppContext::~AppContext() = default;

Status AppContext::loadPricingRules()
{
    auto read = [this](const char *key, qint64 fallback, qint64 &out) -> Status {
        auto value = m_repos->settings.get(QString::fromLatin1(key), QString::number(fallback));
        if (!value)
            return value.status();
        bool ok = false;
        const qint64 v = value.value().toLongLong(&ok);
        out = (ok && v >= 0) ? v : fallback;
        if (!ok || v < 0)
            qWarning() << "Invalid setting" << key << "- using default" << fallback;
        return Status::ok();
    };

    PricingRules rules;
    qint64 flat = rules.shippingFlat.paisa();
    qint64 threshold = rules.freeShippingThreshold.paisa();
    qint64 tax = rules.taxRateBp;
    if (auto st = read("shipping_flat_paisa", flat, flat); !st)
        return st;
    if (auto st = read("free_shipping_threshold_paisa", threshold, threshold); !st)
        return st;
    if (auto st = read("tax_rate_bp", tax, tax); !st)
        return st;

    rules.shippingFlat = Money::fromPaisa(flat);
    rules.freeShippingThreshold = Money::fromPaisa(threshold);
    rules.taxRateBp = static_cast<int>(qMin<qint64>(tax, 10000));
    m_pricing->setRules(rules);
    return Status::ok();
}

} // namespace shophub
