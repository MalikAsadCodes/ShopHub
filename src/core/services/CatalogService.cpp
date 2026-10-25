#include "services/CatalogService.h"

#include "common/Validation.h"
#include "database/Transaction.h"
#include "services/Access.h"

#include <algorithm>

namespace shophub {

namespace {
constexpr int kMaxStockValue = 1'000'000;
const Money kMaxPrice = Money::fromRupees(100'000'000); // 10 crore
}

CatalogService::CatalogService(Repositories &repos, const ImageStore &images)
    : m_repos(repos), m_images(images)
{
}

Product CatalogService::forViewer(const Session &session, Product p)
{
    if (!session.isAdmin())
        p.cost.reset(); // cost price is confidential business data
    return p;
}

Result<ProductPage> CatalogService::search(const Session &session, ProductFilter filter) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    if (!session.isAdmin())
        filter.includeInactive = false;
    if (filter.minPrice && filter.maxPrice && *filter.minPrice > *filter.maxPrice)
        std::swap(filter.minPrice, filter.maxPrice);

    auto page = m_repos.products.search(filter);
    if (!page)
        return page;
    for (Product &p : page.value().items)
        p = forViewer(session, p);
    return page;
}

Result<Product> CatalogService::product(const Session &session, qint64 productId) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    auto found = m_repos.products.findById(productId);
    if (!found)
        return found.status();
    if (!found.value() || (!session.isAdmin() && !found.value()->isPurchasable()))
        return Result<Product>::fail(QStringLiteral("This product is not available."));
    return forViewer(session, *found.value());
}

Result<QVector<Category>> CatalogService::categories(const Session &session, bool includeInactive) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    return m_repos.categories.list(includeInactive && session.isAdmin());
}

Result<QStringList> CatalogService::brands(const Session &session) const
{
    if (auto st = access::requireSignedIn(session); !st)
        return st;
    return m_repos.products.brands();
}

Result<qint64> CatalogService::createCategory(const Session &session, const QString &name)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    const QString n = name.trimmed();
    if (auto st = validation::checkRequired(n, QStringLiteral("Category name"), 60); !st)
        return st;
    auto id = m_repos.categories.insert(n);
    if (id)
        m_repos.audit.log(session.userId(), QStringLiteral("category_created"), QStringLiteral("category"), id.value(), n);
    return id;
}

Status CatalogService::renameCategory(const Session &session, qint64 categoryId, const QString &name)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    const QString n = name.trimmed();
    if (auto st = validation::checkRequired(n, QStringLiteral("Category name"), 60); !st)
        return st;
    auto st = m_repos.categories.rename(categoryId, n);
    if (st)
        m_repos.audit.log(session.userId(), QStringLiteral("category_renamed"), QStringLiteral("category"), categoryId, n);
    return st;
}

Status CatalogService::setCategoryActive(const Session &session, qint64 categoryId, bool active)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto st = m_repos.categories.setActive(categoryId, active);
    if (st)
        m_repos.audit.log(session.userId(), active ? QStringLiteral("category_activated") : QStringLiteral("category_deactivated"),
                          QStringLiteral("category"), categoryId);
    return st;
}

Status CatalogService::validate(ProductInput &in, qint64 excludeProductId) const
{
    in.sku = in.sku.trimmed().toUpper();
    in.name = in.name.trimmed();
    in.brand = in.brand.trimmed();
    in.description = in.description.trimmed();

    if (!validation::isValidSku(in.sku))
        return Status::fail(QStringLiteral("SKU must be 3-32 characters using A-Z, 0-9 and dashes (e.g. ELEC-1001)."));
    if (auto st = validation::checkRequired(in.name, QStringLiteral("Product name"), 120); !st)
        return st;
    if (in.brand.size() > 60)
        return Status::fail(QStringLiteral("Brand must be at most 60 characters."));
    if (in.description.size() > 4000)
        return Status::fail(QStringLiteral("Description must be at most 4000 characters."));
    if (in.price <= Money() || in.price > kMaxPrice)
        return Status::fail(QStringLiteral("Price must be greater than zero."));
    if (in.cost && (in.cost->isNegative() || *in.cost > kMaxPrice))
        return Status::fail(QStringLiteral("Cost price cannot be negative."));
    if (in.initialStock < 0 || in.initialStock > kMaxStockValue)
        return Status::fail(QStringLiteral("Stock must be between 0 and 1,000,000."));
    if (in.lowStockThreshold < 0 || in.lowStockThreshold > kMaxStockValue)
        return Status::fail(QStringLiteral("Low-stock threshold cannot be negative."));
    if (!in.imageFile.isEmpty() && m_images.resolve(in.imageFile).isEmpty())
        return Status::fail(QStringLiteral("The product image could not be found. Please choose it again."));

    auto category = m_repos.categories.findById(in.categoryId);
    if (!category)
        return category.status();
    if (!category.value())
        return Status::fail(QStringLiteral("Please choose a category."));

    auto skuTaken = m_repos.products.skuExists(in.sku, excludeProductId);
    if (!skuTaken)
        return skuTaken.status();
    if (skuTaken.value())
        return Status::fail(QStringLiteral("Another product already uses SKU %1.").arg(in.sku));
    return Status::ok();
}

Result<qint64> CatalogService::createProduct(const Session &session, ProductInput input)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    if (auto st = validate(input, 0); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    auto id = m_repos.products.insert(input, input.initialStock);
    if (!id)
        return id;
    if (input.initialStock > 0) {
        if (auto st = m_repos.products.insertMovement(id.value(), input.initialStock, input.initialStock,
                                                      StockReason::Initial, QString(), session.userId());
            !st)
            return st;
    }
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("product_created"), QStringLiteral("product"),
                                    id.value(), input.sku);
        !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;
    return id;
}

Status CatalogService::updateProduct(const Session &session, qint64 productId, ProductInput input)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto existing = m_repos.products.findById(productId);
    if (!existing)
        return existing.status();
    if (!existing.value())
        return Status::fail(QStringLiteral("Product not found."));
    if (auto st = validate(input, productId); !st)
        return st;

    auto st = m_repos.products.update(productId, input);
    if (st) {
        QString details = input.sku;
        if (existing.value()->price != input.price)
            details += QStringLiteral(" price %1 -> %2").arg(existing.value()->price.toString(), input.price.toString());
        if (existing.value()->qistAllowed != input.qistAllowed)
            details += input.qistAllowed ? QStringLiteral(" qist on") : QStringLiteral(" qist off");
        m_repos.audit.log(session.userId(), QStringLiteral("product_updated"), QStringLiteral("product"), productId, details);
    }
    return st;
}

Status CatalogService::setProductActive(const Session &session, qint64 productId, bool active)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto st = m_repos.products.setActive(productId, active);
    if (st)
        m_repos.audit.log(session.userId(), active ? QStringLiteral("product_activated") : QStringLiteral("product_deactivated"),
                          QStringLiteral("product"), productId);
    return st;
}

Status CatalogService::setQistAllowed(const Session &session, qint64 productId, bool allowed)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto existing = m_repos.products.findById(productId);
    if (!existing)
        return existing.status();
    if (!existing.value())
        return Status::fail(QStringLiteral("Product not found."));
    auto st = m_repos.products.setQistAllowed(productId, allowed);
    if (st)
        m_repos.audit.log(session.userId(), allowed ? QStringLiteral("product_qist_on") : QStringLiteral("product_qist_off"),
                          QStringLiteral("product"), productId, existing.value()->sku);
    return st;
}

Result<QString> CatalogService::importImage(const Session &session, const QString &sourcePath)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    return m_images.import(sourcePath);
}

// ---------------------------------------------------------------- wholesale prices

namespace {

// "12+: 5%, 50+: 10%" for the audit log.
QString describeTiers(const QVector<PriceTier> &tiers)
{
    if (tiers.isEmpty())
        return QStringLiteral("none");
    QStringList parts;
    for (const PriceTier &t : tiers)
        parts << QStringLiteral("%1+: %2%").arg(t.minQuantity).arg(pricetier::percentText(t.discountBp));
    return parts.join(QStringLiteral(", "));
}

} // namespace

Result<QVector<PriceTier>> CatalogService::priceTiers(const Session &session, qint64 productId) const
{
    // product() checks the session and hides products a customer may not see.
    if (auto p = product(session, productId); !p)
        return p.status();
    return m_repos.products.tiersFor(productId);
}

Status CatalogService::setPriceTiers(const Session &session, qint64 productId, QVector<PriceTier> tiers)
{
    if (auto st = access::requireAdmin(session); !st)
        return st;
    auto existing = m_repos.products.findById(productId);
    if (!existing)
        return existing.status();
    if (!existing.value())
        return Status::fail(QStringLiteral("Product not found."));
    if (const QString problem = pricetier::validate(tiers); !problem.isEmpty())
        return Status::fail(problem);

    std::sort(tiers.begin(), tiers.end(),
              [](const PriceTier &a, const PriceTier &b) { return a.minQuantity < b.minQuantity; });

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;
    if (auto st = m_repos.products.replaceTiers(productId, tiers); !st)
        return st;
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("product_tiers_updated"), QStringLiteral("product"),
                                    productId, existing.value()->sku + QStringLiteral(" ") + describeTiers(tiers));
        !st)
        return st;
    return tx.commit();
}

QStringList CatalogService::belowCostWarnings(Money price, const std::optional<Money> &cost,
                                              const QVector<PriceTier> &tiers)
{
    QStringList out;
    if (!cost)
        return out;
    QVector<PriceTier> sorted = tiers;
    std::sort(sorted.begin(), sorted.end(),
              [](const PriceTier &a, const PriceTier &b) { return a.minQuantity < b.minQuantity; });
    for (const PriceTier &t : sorted) {
        const Money unit = pricetier::unitPrice(price, t);
        if (unit < *cost)
            out << QStringLiteral("%1 or more: %2 each is below the cost price (%3).")
                       .arg(t.minQuantity)
                       .arg(unit.toString(), cost->toString());
    }
    return out;
}

} // namespace shophub
