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

private:
    Status validate(ProductInput &input, qint64 excludeProductId) const;
    static Product forViewer(const Session &session, Product p);

    Repositories &m_repos;
    const ImageStore &m_images;
};

} // namespace shophub
