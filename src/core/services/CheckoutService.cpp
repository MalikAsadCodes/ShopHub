#include "services/CheckoutService.h"

#include "common/Validation.h"
#include "database/SqlUtil.h"
#include "database/Transaction.h"
#include "services/Access.h"
#include "services/CartService.h"

#include <QDate>
#include <QRandomGenerator>
#include <QUuid>
#include <QtDebug>

namespace shophub {

CheckoutService::CheckoutService(Repositories &repos, const PricingService &pricing)
    : m_repos(repos), m_pricing(pricing)
{
}

QString CheckoutService::newCheckoutToken()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString CheckoutService::newOrderNumber()
{
    // e.g. SH-20261017-7K3QX9 (date + 6 random characters from a secure source)
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no 0/O/1/I confusion
    QString suffix;
    for (int i = 0; i < 6; ++i)
        suffix.append(QLatin1Char(alphabet[QRandomGenerator::system()->bounded(32)]));
    return QStringLiteral("SH-%1-%2").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")), suffix);
}

Result<std::optional<Address>> CheckoutService::savedAddress(const Session &session) const
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    return m_repos.addresses.defaultFor(session.userId());
}

Status CheckoutService::validateShipping(const ShippingDetails &s) const
{
    if (auto st = validation::checkRequired(s.fullName, QStringLiteral("Full name"), 100); !st)
        return st;
    if (!validation::isValidPhone(s.phone))
        return Status::fail(QStringLiteral("Please enter a valid phone number (e.g. 0300 1234567)."));
    if (auto st = validation::checkRequired(s.line1, QStringLiteral("Address"), 250); !st)
        return st;
    if (auto st = validation::checkRequired(s.city, QStringLiteral("City"), 60); !st)
        return st;
    return Status::ok();
}

Status CheckoutService::injectFault(const QString &stage) const
{
    return m_faultInjector ? m_faultInjector(stage) : Status::ok();
}

Result<Order> CheckoutService::placeOrder(const Session &session, const CheckoutRequest &req)
{
    if (auto st = access::requireCustomer(session); !st)
        return st;
    if (req.checkoutToken.trimmed().isEmpty())
        return Result<Order>::fail(QStringLiteral("Checkout session expired. Please open checkout again."));
    const bool onKhata = req.paymentMethod == payment::kKhata;
    if (req.paymentMethod != payment::kCashOnDelivery && !onKhata)
        return Result<Order>::fail(QStringLiteral("Please choose Cash on Delivery or Khata."));

    ShippingDetails ship = req.shipping;
    ship.fullName = ship.fullName.trimmed();
    ship.phone = ship.phone.trimmed();
    ship.line1 = ship.line1.trimmed();
    ship.city = ship.city.trimmed();
    if (auto st = validateShipping(ship); !st)
        return st;

    Transaction tx(m_repos.connection);
    if (auto st = tx.begin(); !st)
        return st;

    // 1. Duplicate submission? Return the order that was already placed.
    auto existing = m_repos.orders.findIdByToken(req.checkoutToken);
    if (!existing)
        return existing.status();
    if (existing.value()) {
        tx.rollback();
        auto order = m_repos.orders.findById(*existing.value(), true);
        if (!order)
            return order.status();
        if (!order.value() || order.value()->userId != session.userId())
            return Result<Order>::fail(QStringLiteral("Checkout session expired. Please open checkout again."));
        qInfo() << "Duplicate checkout submission ignored for order" << order.value()->orderNumber;
        return *order.value();
    }

    // 2. Re-read the cart inside the transaction and check stock.
    auto cart = m_repos.carts.items(session.userId());
    if (!cart)
        return cart.status();
    const QVector<CartItem> items = cart.value();
    if (items.isEmpty())
        return Result<Order>::fail(QStringLiteral("Your cart is empty."));
    const QStringList problems = CartService::problems(items);
    if (!problems.isEmpty())
        return Result<Order>::fail(problems.join(QLatin1Char('\n')));

    // 3. Price it here, in the business layer.
    const PriceBreakdown price = m_pricing.price(items);

    Order order;
    order.userId = session.userId();
    order.status = OrderStatus::Pending;
    order.subtotal = price.subtotal;
    order.shipping = price.shipping;
    order.tax = price.tax;
    order.total = price.total;
    order.paymentMethod = req.paymentMethod;
    order.paymentStatus = onKhata ? payment::kOnAccount : payment::kUnpaid;

    // 3b. Khata: check the credit limit here, inside the transaction, so two
    //     orders can never both squeeze under the limit.
    if (onKhata) {
        auto account = m_repos.khata.account(session.userId());
        if (!account)
            return account.status();
        if (!account.value() || account.value()->creditLimit.isZero())
            return Result<Order>::fail(QStringLiteral(
                "Khata is not open for your account. Please choose Cash on Delivery or ask the shop to give you a credit limit."));
        const KhataAccount &k = *account.value();
        if (k.balance + price.total > k.creditLimit)
            return Result<Order>::fail(
                QStringLiteral("This order (%1) is more than your available khata credit (%2). "
                               "Your limit is %3 and you owe %4. Please choose Cash on Delivery, "
                               "or pay some of your khata first.")
                    .arg(price.total.toString(), k.available().toString(), k.creditLimit.toString(),
                         k.balance.toString()));
    }
    order.shipName = ship.fullName;
    order.shipPhone = ship.phone;
    order.shipLine1 = ship.line1;
    order.shipCity = ship.city;
    order.checkoutToken = req.checkoutToken;
    for (const CartItem &c : items) {
        OrderItem line;
        line.productId = c.productId;
        line.sku = c.sku;
        line.productName = c.name;
        line.unitPrice = c.unitPrice;
        line.quantity = c.quantity;
        line.lineTotal = c.lineTotal();
        order.items.push_back(line);
    }

    // 4. Insert the order (retry if the random order number collides).
    Result<qint64> orderId = Result<qint64>::fail(QString());
    for (int attempt = 0; attempt < 3; ++attempt) {
        order.orderNumber = newOrderNumber();
        orderId = m_repos.orders.insert(order);
        if (orderId)
            break;
    }
    if (!orderId)
        return orderId.status();
    if (auto st = injectFault(QStringLiteral("after_order_insert")); !st)
        return st;

    // 5. Deduct stock. The UPDATE refuses to go below zero, so this is safe even
    //    if stock changed since the cart was loaded.
    for (const CartItem &c : items) {
        auto newStock = m_repos.products.changeStock(c.productId, -c.quantity);
        if (!newStock)
            return Result<Order>::fail(QStringLiteral("%1 just went out of stock. Please update your cart.").arg(c.name));
        if (auto st = m_repos.products.insertMovement(c.productId, -c.quantity, newStock.value(), StockReason::Sale,
                                                      order.orderNumber, session.userId());
            !st)
            return st;
    }
    if (auto st = injectFault(QStringLiteral("after_stock_update")); !st)
        return st;

    // 5b. Khata: the order goes into the ledger in the same transaction.
    if (onKhata) {
        auto after = m_repos.khata.append(session.userId(), KhataKind::Charge, price.total, orderId.value(),
                                          order.orderNumber, session.userId());
        if (!after)
            return after.status();
        if (auto st = injectFault(QStringLiteral("after_khata_charge")); !st)
            return st;
    }

    // 6. History, cart, address, audit.
    if (auto st = m_repos.orders.addHistory(orderId.value(), std::nullopt, OrderStatus::Pending, session.userId(),
                                            QStringLiteral("Order placed (%1)").arg(payment::methodDisplay(req.paymentMethod)));
        !st)
        return st;
    if (auto st = m_repos.carts.clear(session.userId()); !st)
        return st;
    if (req.saveAsDefaultAddress) {
        Address a;
        a.userId = session.userId();
        a.fullName = ship.fullName;
        a.phone = ship.phone;
        a.line1 = ship.line1;
        a.city = ship.city;
        if (auto st = m_repos.addresses.saveDefault(a); !st)
            return st;
    }
    if (auto st = m_repos.audit.log(session.userId(), QStringLiteral("order_placed"), QStringLiteral("order"),
                                    orderId.value(), order.orderNumber);
        !st)
        return st;

    if (auto st = injectFault(QStringLiteral("before_commit")); !st)
        return st;
    if (auto st = tx.commit(); !st)
        return st;

    qInfo().noquote() << "Order placed" << order.orderNumber << "total" << order.total.toString();
    auto saved = m_repos.orders.findById(orderId.value(), true);
    if (!saved)
        return saved.status();
    if (!saved.value())
        return Result<Order>::fail(sql::kGenericDbError);
    return *saved.value();
}

} // namespace shophub
