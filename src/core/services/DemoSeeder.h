#pragma once

#include "common/Result.h"
#include "domain/Session.h"

#include <QVector>

namespace shophub {

class AppContext;

// Optional sample data for trying the app: categories, 27 products, 50 customer
// accounts with Pakistani names and about 70 orders in every state over the last
// three weeks. Products and orders go through the normal services, so they follow
// every business rule. No sample ADMIN account is ever created.
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

    static QVector<SampleAccount> sampleCustomers();
    static int sampleOrderCount();

    static Status seed(AppContext &ctx, const Session &admin);
    static bool isLoaded(AppContext &ctx);
};

} // namespace shophub
