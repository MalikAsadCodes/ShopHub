#pragma once

// Shared helpers for the integration tests: a fresh in-memory back end
// with a fast password hasher, plus shortcuts for common fixtures.

#include "services/AppContext.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace shophub::test {

struct Backend
{
    QTemporaryDir imageDir;
    std::unique_ptr<AppContext> ctx;
    Session admin;
    Session customer;
    qint64 categoryId = 0;

    // Creates the context plus an admin, a customer and one category.
    bool init()
    {
        AppConfig cfg;
        cfg.databasePath = QStringLiteral(":memory:");
        cfg.imageDirectory = imageDir.path();
        cfg.passwordIterations = 1000; // fast for tests; production uses 210,000
        auto created = AppContext::create(cfg);
        if (!created) {
            qWarning() << created.error();
            return false;
        }
        ctx = created.takeValue();

        auto a = ctx->auth().createFirstAdmin(QStringLiteral("Admin User"), QStringLiteral("admin@test.pk"),
                                              QStringLiteral("AdminPass1"));
        auto c = ctx->auth().registerCustomer(QStringLiteral("Test Customer"), QStringLiteral("cust@test.pk"),
                                              QStringLiteral("CustPass1"));
        if (!a || !c)
            return false;
        admin = a.value();
        customer = c.value();

        auto cat = ctx->catalog().createCategory(admin, QStringLiteral("Electronics"));
        if (!cat)
            return false;
        categoryId = cat.value();
        return true;
    }

    qint64 addProduct(const QString &sku, qint64 priceRupees, int stock, const QString &name = QString())
    {
        ProductInput in;
        in.sku = sku;
        in.name = name.isEmpty() ? QStringLiteral("Product ") + sku : name;
        in.categoryId = categoryId;
        in.brand = QStringLiteral("TestBrand");
        in.price = Money::fromRupees(priceRupees);
        in.cost = Money::fromRupees(priceRupees / 2);
        in.initialStock = stock;
        auto id = ctx->catalog().createProduct(admin, in);
        if (!id)
            qWarning() << "addProduct failed:" << id.error();
        return id ? id.value() : 0;
    }

    int stockOf(qint64 productId)
    {
        auto p = ctx->catalog().product(admin, productId);
        return p ? p.value().stock : -1;
    }

    // Runs a single-value SQL query directly, for assertions only.
    QVariant scalar(const QString &sql)
    {
        QSqlQuery q(QSqlDatabase::database(ctx->connectionName(), false));
        if (!q.exec(sql) || !q.next())
            return QVariant();
        return q.value(0);
    }

    CheckoutRequest checkoutRequest() const
    {
        CheckoutRequest r;
        r.checkoutToken = CheckoutService::newCheckoutToken();
        r.shipping = {QStringLiteral("Test Customer"), QStringLiteral("0300 1234567"),
                      QStringLiteral("House 1, Street 2"), QStringLiteral("Islamabad")};
        return r;
    }
};

} // namespace shophub::test
