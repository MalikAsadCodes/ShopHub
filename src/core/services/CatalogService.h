#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"
#include "services/ImageStore.h"

namespace shophub {

class CatalogService
{
public:
    CatalogService(Repositories &repos, const ImageStore &images);

    // ---- browsing (any signed-in user)
    Result<ProductPage> search(const Session &session, ProductFilter filter) const;
    Result<Product> product(const Session &session, qint64 productId) const;
    Result<QVector<Category>> categories(const Session &session, bool includeInactive = false) const;
    Result<QStringList> brands(const Session &session) const;

    // ---- administration
    Result<qint64> createCategory(const Session &session, const QString &name);
    Status renameCategory(const Session &session, qint64 categoryId, const QString &name);
    Status setCategoryActive(const Session &session, qint64 categoryId, bool active);

    Result<qint64> createProduct(const Session &session, ProductInput input);
    Status updateProduct(const Session &session, qint64 productId, ProductInput input);
    Status setProductActive(const Session &session, qint64 productId, bool active);
    Result<QString> importImage(const Session &session, const QString &sourcePath);

    // ---- wholesale (quantity) prices, see docs/WHOLESALE.md
    // Anyone signed in may read the tiers of a product they can see.
    Result<QVector<PriceTier>> priceTiers(const Session &session, qint64 productId) const;
    // Admin only. Replaces all tiers of the product (an empty list removes them).
    // The list is checked with pricetier::validate and saved with an audit entry.
    Status setPriceTiers(const Session &session, qint64 productId, QVector<PriceTier> tiers);
    // Admin hint, not an error: one message per tier whose unit price falls
    // below the product's cost price. Empty when there is no cost price.
    static QStringList belowCostWarnings(Money price, const std::optional<Money> &cost,
                                         const QVector<PriceTier> &tiers);

private:
    Status validate(ProductInput &input, qint64 excludeProductId) const;
    static Product forViewer(const Session &session, Product p);

    Repositories &m_repos;
    const ImageStore &m_images;
};

} // namespace shophub
