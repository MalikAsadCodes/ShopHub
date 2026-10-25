#pragma once

#include "common/Result.h"
#include "domain/Session.h"

#include <QVector>

namespace shophub {

class AppContext;

// Optional sample data for trying the app: categories, 27 products (6 with
// wholesale prices, 10 sold on qist), 50 customer accounts with Pakistani names,
// 80 orders in every state (3 bulk orders at wholesale prices, 5 on qist), eight
// customers with a khata (credit) account and six allowed qist. Products, orders,
// khata entries and qists go through the normal services, so they follow every
// business rule. No sample ADMIN account is ever created.
//
// The sample data has a version. A database set up with older sample data is
// brought up to date by upgrade() the next time the admin signs in, so new
// features have sample data in existing databases too. A real shop (no sample
// data) is never touched.
//   version 1: products, customers, orders, khata (ShopHub v1 and v2 milestone 1)
//   version 2: wholesale prices and 3 bulk orders (v2 milestone 2)
//   version 3: qist plans, products, customers and 5 qists (v2 milestone 3)
//
// The full account list is documented in docs/CUSTOMER_ACCOUNTS.md and
// docs/customer_accounts.csv; a test keeps those files in sync with this class.
class DemoSeeder
{
public:
    struct SampleAccount
    {
        QString fullName;
        QString email;      // firstname.lastname@example.com
        QString password;   // first name + "123", e.g. "Ayesha123"
        QString phone;
        QString address;
        QString city;
    };

    // The account shown as a hint on the sign-in screen.
    static inline const QString kCustomerEmail = QStringLiteral("ayesha.khan@example.com");
    static inline const QString kCustomerPassword = QStringLiteral("Ayesha123");

    static constexpr int kDataVersion = 3;

    static QVector<SampleAccount> sampleCustomers();
    static int sampleOrderCount();
    static int sampleKhataCustomerCount(); // customers given a khata credit limit
    static int sampleWholesaleProductCount(); // products given quantity prices
    static int sampleQistPlanCount();
    static int sampleQistProductCount();   // products that may be sold on qist
    static int sampleQistCustomerCount();  // customers allowed qist
    static int sampleQistCount();          // qists already sold

    // New sample data. `version` is for tests only: an older version builds the
    // sample data an older ShopHub did, so upgrade() can be tested on it.
    static Status seed(AppContext &ctx, const Session &admin, int version = kDataVersion);
    static bool isLoaded(AppContext &ctx);

    // 0 = no sample data, otherwise the version it is at.
    static int dataVersion(AppContext &ctx);
    static bool needsUpgrade(AppContext &ctx);
    // Adds what newer versions bring to older sample data, once. Keeps everything
    // the shop changed: deleted products or customers, prices it set, orders.
    static Status upgrade(AppContext &ctx, const Session &admin);
};

} // namespace shophub
