#pragma once

#include "common/Result.h"
#include "domain/Session.h"
#include "repositories/Repositories.h"
#include "services/PricingService.h"

#include <functional>

namespace shophub {

struct ShippingDetails
{
    QString fullName;
    QString phone;
    QString line1;
    QString city;
};

struct CheckoutRequest
{
    // Generated once when the checkout screen opens. Submitting the same
    // token twice (double-click, retry) returns the first order instead of
    // creating a duplicate.
    QString checkoutToken;
    ShippingDetails shipping;
    QString paymentMethod = QStringLiteral("cod"); // "cod" or "khata" (needs a credit limit)
    bool saveAsDefaultAddress = true;
};

class CheckoutService
{
public:
    using FaultInjector = std::function<Status(const QString &stage)>;

    CheckoutService(Repositories &repos, const PricingService &pricing);

    static QString newCheckoutToken();

    Result<std::optional<Address>> savedAddress(const Session &session) const;

    // Validates the cart and places the order in ONE database transaction:
    // order + lines + stock deduction + stock movements + cart clearing.
    // Either everything is saved or nothing is.
    Result<Order> placeOrder(const Session &session, const CheckoutRequest &request);

    // Tests only: lets a test force a failure at a named stage to prove rollback.
    void setFaultInjectorForTesting(FaultInjector injector) { m_faultInjector = std::move(injector); }

private:
    Status validateShipping(const ShippingDetails &s) const;
    Status injectFault(const QString &stage) const;
    static QString newOrderNumber();

    Repositories &m_repos;
    const PricingService &m_pricing;
    FaultInjector m_faultInjector;
};

} // namespace shophub
