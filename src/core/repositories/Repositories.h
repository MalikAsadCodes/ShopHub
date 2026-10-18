#pragma once

#include "repositories/CartRepository.h"
#include "repositories/CategoryRepository.h"
#include "repositories/MiscRepositories.h"
#include "repositories/OrderRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/UserRepository.h"

namespace shophub {

// All repositories for one database connection, handed to the services.
struct Repositories
{
    explicit Repositories(const QString &connectionName)
        : connection(connectionName),
          users(connectionName),
          categories(connectionName),
          products(connectionName),
          carts(connectionName),
          orders(connectionName),
          addresses(connectionName),
          audit(connectionName),
          settings(connectionName)
    {}

    QString connection; // used by services to open transactions
    UserRepository users;
    CategoryRepository categories;
    ProductRepository products;
    CartRepository carts;
    OrderRepository orders;
    AddressRepository addresses;
    AuditRepository audit;
    SettingsRepository settings;
};

} // namespace shophub
