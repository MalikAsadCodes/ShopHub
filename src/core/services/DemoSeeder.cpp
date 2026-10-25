#include "services/DemoSeeder.h"

#include "database/SqlUtil.h"
#include "services/AppContext.h"

#include <QHash>
#include <QSet>
#include <QSqlQuery>
#include <QtDebug>

#include <iterator>

namespace shophub {

namespace {

struct DemoProduct
{
    const char *category;
    const char *sku;
    const char *name;
    const char *brand;
    qint64 priceRupees;
    qint64 costRupees;
    int stock;
    const char *description;
};

// Brands are fictional, apart from book publishers.
const DemoProduct kProducts[] = {
    {"Electronics", "ELEC-1001", "Wireless Earbuds Pro", "SoundWave", 6499, 4200, 40,
     "True wireless earbuds with active noise cancellation, 30-hour battery with the case and USB-C charging."},
    {"Electronics", "ELEC-1002", "Smart Watch Series 5", "Pulse", 12999, 8900, 25,
     "1.8\" AMOLED display, heart-rate and SpO2 tracking, 100+ workout modes and 7-day battery life."},
    {"Electronics", "ELEC-1003", "20000mAh Power Bank", "VoltX", 7999, 5600, 3,
     "High-capacity power bank with 22.5W fast charging and two USB-A plus one USB-C port."},
    {"Electronics", "ELEC-1004", "Bluetooth Speaker Mini", "Boom", 9499, 6500, 18,
     "Pocket-size waterproof (IPX7) speaker with deep bass and 12 hours of playtime."},
    {"Electronics", "ELEC-1005", "USB-C Fast Charger 65W", "ChargeIt", 3999, 2400, 60,
     "GaN charger that powers laptops, tablets and phones. Includes a 1.5 m USB-C cable."},
    {"Electronics", "ELEC-1006", "Mechanical Keyboard", "KeyForge", 11500, 8000, 0,
     "Tenkeyless mechanical keyboard with hot-swappable switches and white backlight."},
    {"Fashion", "FASH-2001", "Men's Cotton Kurta", "Indus Threads", 4250, 2300, 35,
     "Breathable cotton kurta with a classic band collar. Machine washable."},
    {"Fashion", "FASH-2002", "Women's Lawn Suit (3-Piece)", "Lahore Loom", 6990, 4100, 22,
     "Printed lawn shirt, dyed trousers and chiffon dupatta. Unstitched."},
    {"Fashion", "FASH-2003", "Leather Peshawari Chappal", "Peshawar Craft", 5500, 3200, 15,
     "Handmade genuine leather chappal with a cushioned sole."},
    {"Fashion", "FASH-2004", "Denim Jacket", "Urban Denim", 7490, 4300, 4,
     "Classic fit stonewashed denim jacket with button-flap pockets."},
    {"Fashion", "FASH-2005", "Running Sneakers", "StrideOn", 8999, 5200, 28,
     "Lightweight mesh sneakers with responsive foam for daily runs."},
    {"Home & Kitchen", "HOME-3001", "Non-Stick Cookware Set (7 pcs)", "ChefLine", 14999, 9800, 12,
     "Granite-coated pots and pans with glass lids. Suitable for gas and electric stoves."},
    {"Home & Kitchen", "HOME-3002", "Electric Kettle 1.7L", "HomeEase", 4599, 2900, 30,
     "Stainless steel kettle with auto shut-off and boil-dry protection."},
    {"Home & Kitchen", "HOME-3003", "Ceramic Dinner Set (32 pcs)", "Glaze & Co", 18500, 12000, 6,
     "Elegant ceramic dinner set for six, microwave and dishwasher safe."},
    {"Home & Kitchen", "HOME-3004", "Air Fryer 4.5L", "CrispAir", 21999, 15500, 9,
     "Cook with up to 85% less oil. Digital controls and 8 preset programs."},
    {"Home & Kitchen", "HOME-3005", "Cotton Bedsheet Set (King)", "SoftNest", 5999, 3500, 20,
     "200 thread-count cotton bedsheet with two pillow covers."},
    {"Books", "BOOK-4001", "Clean Code", "Prentice Hall", 2850, 1900, 14,
     "A handbook of agile software craftsmanship by Robert C. Martin."},
    {"Books", "BOOK-4002", "Atomic Habits", "Penguin", 1950, 1200, 45,
     "James Clear's guide to building good habits and breaking bad ones."},
    {"Books", "BOOK-4003", "The Pragmatic Programmer", "Addison-Wesley", 3200, 2100, 2,
     "Classic advice for software developers, 20th anniversary edition."},
    {"Books", "BOOK-4004", "Introduction to Algorithms", "MIT Press", 6750, 4800, 8,
     "The comprehensive algorithms textbook (CLRS), 4th edition."},
    {"Sports & Fitness", "SPRT-5001", "English Willow Cricket Bat", "WillowKing", 15500, 10200, 10,
     "Grade 2 English willow bat with a thick edge and cane handle."},
    {"Sports & Fitness", "SPRT-5002", "Yoga Mat 6mm", "FlexFit", 2499, 1300, 40,
     "Non-slip TPE yoga mat with a carry strap."},
    {"Sports & Fitness", "SPRT-5003", "Adjustable Dumbbell Set 20kg", "IronCore", 9999, 7000, 7,
     "Pair of adjustable dumbbells with chrome handles and secure collars."},
    {"Sports & Fitness", "SPRT-5004", "Match Football Size 5", "GoalPro", 3299, 1800, 25,
     "Hand-stitched match ball made in Sialkot."},
    {"Beauty & Care", "BEAU-6001", "Herbal Face Wash", "Glow", 899, 450, 80,
     "Gentle neem and aloe vera face wash for daily use."},
    {"Beauty & Care", "BEAU-6002", "Saffron Hair Oil", "Roots", 1250, 700, 50,
     "Nourishing blend of saffron, almond and coconut oils."},
    {"Beauty & Care", "BEAU-6003", "Oud Perfume 50ml", "Attar House", 4999, 2600, 5,
     "Long-lasting eau de parfum with warm oud and amber notes."},
};

// Wholesale (quantity) prices on items shops and clubs buy in bulk. Every tier
// starts above the quantities in the regular sample orders below, so only the
// three bulk orders at the end of sampleOrders() use them, and the khata
// figures in docs/CUSTOMER_ACCOUNTS.md stay the same.
struct DemoTier
{
    const char *sku;
    int minQuantity;
    int discountBp;
};
const DemoTier kTiers[] = {
    {"BEAU-6001", 6, 500},  {"BEAU-6001", 12, 1000},                         // face wash: salons, hostels
    {"BEAU-6002", 6, 500},  {"BEAU-6002", 12, 800},                          // hair oil
    {"BOOK-4002", 10, 1000},                                                  // book clubs, schools
    {"SPRT-5002", 5, 500},                                                    // yoga mats for a class
    {"SPRT-5004", 6, 500},  {"SPRT-5004", 12, 1000}, {"SPRT-5004", 24, 1500}, // Sialkot footballs for clubs
    {"ELEC-1005", 10, 800},                                                   // chargers for offices
};

// Sample customers: first name, last name, phone, address, city.
// Email = firstname.lastname@example.com, password = first name + "123".
// Keep docs/CUSTOMER_ACCOUNTS.md and docs/customer_accounts.csv in step with this list
// (tst_orders_inventory fails if they differ).
struct SampleCustomerRow
{
    const char *firstName;
    const char *lastName;
    const char *phone;
    const char *address;
    const char *city;
};

const SampleCustomerRow kCustomers[] = {
    {"Ayesha", "Khan", "0300 1234567", "House 12, Street 4, F-7/2", "Islamabad"},
    {"Muhammad", "Asad", "0312 4587210", "House 7, Lane 3, Chaklala Scheme III", "Rawalpindi"},
    {"Akram", "Javed", "0301 6623945", "P-118, Madina Town", "Faisalabad"},
    {"Humayun", "Aslam", "0333 9182746", "House 41, Street 9, Hayatabad Phase 3", "Peshawar"},
    {"Aimal", "Durrani", "0345 2847193", "House 22, Jinnah Town", "Quetta"},
    {"Rahab", "Fatima", "0321 5573821", "88-C, Johar Town", "Lahore"},
    {"Bilal", "Ahmed", "0321 7654321", "45-B Gulberg III", "Lahore"},
    {"Fatima", "Noor", "0300 2219834", "Flat 14, Block 5, Clifton", "Karachi"},
    {"Hamza", "Tariq", "0305 7718342", "House 19, Gulgasht Colony", "Multan"},
    {"Zainab", "Hussain", "0331 4401927", "B-201, Gulshan-e-Iqbal Block 13-D", "Karachi"},
    {"Usman", "Ghani", "0302 6650381", "House 5, Model Town", "Sialkot"},
    {"Hira", "Qureshi", "0334 1296573", "House 61, Latifabad Unit 7", "Hyderabad"},
    {"Saad", "Iqbal", "0300 8834521", "House 9, Street 22, G-11/3", "Islamabad"},
    {"Maryam", "Siddiqui", "0322 3348716", "A-77, North Nazimabad Block H", "Karachi"},
    {"Ali", "Haider", "0311 5092846", "House 30, Satellite Town", "Rawalpindi"},
    {"Sana", "Butt", "0306 2275149", "House 14, Satellite Town", "Gujranwala"},
    {"Faisal", "Mehmood", "0323 8814270", "112-E, DHA Phase 5", "Lahore"},
    {"Amna", "Riaz", "0300 4417785", "House 3, Peoples Colony No. 1", "Faisalabad"},
    {"Hassan", "Raza", "0313 9925514", "House 52, Shah Rukn-e-Alam Colony", "Multan"},
    {"Iqra", "Shahid", "0324 6672038", "21-A, Model Town", "Lahore"},
    {"Zubair", "Anwar", "0335 3381904", "House 18, PECHS Block 2", "Karachi"},
    {"Mahnoor", "Abbasi", "0346 1179352", "House 6, Jinnah Abad", "Abbottabad"},
    {"Kashif", "Nadeem", "0307 5528461", "House 27, Satellite Town", "Sargodha"},
    {"Rabia", "Aziz", "0301 9047628", "House 44, Street 12, E-11/2", "Islamabad"},
    {"Imran", "Shafiq", "0300 7731590", "House 10, Model Town A", "Bahawalpur"},
    {"Laiba", "Farooq", "0314 2268457", "House 63, Bahria Town Phase 7", "Rawalpindi"},
    {"Danish", "Rasheed", "0336 8805213", "C-45, DHA Phase 6", "Karachi"},
    {"Nimra", "Zafar", "0325 4416739", "House 90, Wapda Town", "Lahore"},
    {"Waqas", "Yousaf", "0308 3359172", "House 15, Shadman Colony", "Gujrat"},
    {"Sidra", "Kamran", "0300 6684301", "House 8, Street 31, F-10/1", "Islamabad"},
    {"Junaid", "Akhtar", "0347 9913648", "House 70, University Town", "Peshawar"},
    {"Mehwish", "Saleem", "0332 2257490", "Flat 3, Block 7, Federal B Area", "Karachi"},
    {"Adeel", "Sheikh", "0320 5593817", "House 25, Garden Town", "Lahore"},
    {"Areeba", "Naveed", "0303 4478126", "House 12, Cantt", "Multan"},
    {"Talha", "Asghar", "0315 7746209", "House 38, Westridge", "Rawalpindi"},
    {"Kiran", "Jamil", "0337 1182934", "House 47, Qasimabad", "Hyderabad"},
    {"Fahad", "Latif", "0331 6620475", "B-12, Bahadurabad", "Karachi"},
    {"Hafsa", "Irfan", "0326 3315864", "House 101, Faisal Town", "Lahore"},
    {"Shahzaib", "Malik", "0301 8857312", "House 16, Street 7, I-8/3", "Islamabad"},
    {"Anum", "Rehman", "0309 2241697", "House 33, Susan Road", "Faisalabad"},
    {"Owais", "Chaudhry", "0316 5583042", "House 11, Cantt", "Sialkot"},
    {"Saima", "Bashir", "0348 7726415", "House 29, Satellite Town", "Quetta"},
    {"Zeeshan", "Arif", "0333 4492867", "R-56, Gulistan-e-Johar Block 15", "Karachi"},
    {"Javeria", "Khalid", "0317 9931258", "House 54, Askari 14", "Rawalpindi"},
    {"Umar", "Daud", "0327 6648093", "House 72, Allama Iqbal Town", "Lahore"},
    {"Bushra", "Nasir", "0302 3376541", "House 5, Street 18, G-9/2", "Islamabad"},
    {"Rizwan", "Haq", "0318 8813720", "House 20, Bosan Road", "Multan"},
    {"Nadia", "Pervaiz", "0338 5527169", "A-9, Gulshan-e-Iqbal Block 6", "Karachi"},
    {"Tahir", "Mirza", "0304 1194586", "House 41, Peoples Colony", "Gujranwala"},
    {"Sadia", "Ijaz", "0349 6603274", "House 13, Hayatabad Phase 6", "Peshawar"},
};

const char *const kCancelReasons[] = {
    "Ordered the wrong size", "Found it cheaper elsewhere", "Ordered by mistake",
    "Delivery would take too long", "Changed my mind",
};

struct DemoOrder
{
    int customer;
    QVector<QPair<const char *, int>> items;
    OrderStatus finalStatus;
    int daysAgo;
};

// When sample order number `index` was placed. Orders placed on the same day are
// spread over different times of day.
QDateTime sampleTime(int daysAgo, int index)
{
    const qint64 minutesBack = daysAgo == 0 ? 5 + (index * 7) % 50     // today: within the last hour
                                            : (index * 97) % (10 * 60); // earlier days: up to 10 hours earlier
    return QDateTime::currentDateTimeUtc().addDays(-daysAgo).addSecs(-60 * minutesBack);
}

// Moves an order (and its status history) back in time, including the date inside
// its order number (SH-yyyyMMdd-XXXXXX).
Status backdateOrder(const QString &connection, qint64 orderId, const QDateTime &whenUtc)
{
    const QString when = sql::toDb(whenUtc);
    const QString numberPrefix = QStringLiteral("SH-") + whenUtc.toLocalTime().date().toString(QStringLiteral("yyyyMMdd"));

    const char *const statements[] = {
        "UPDATE orders SET created_at = :t, updated_at = :t2, order_number = :p || substr(order_number, 12) WHERE id = :id",
        "UPDATE order_status_history SET changed_at = :t WHERE order_id = :id",
    };
    for (const char *statement : statements) {
        const QString text = QString::fromLatin1(statement);
        QSqlQuery q(QSqlDatabase::database(connection, false));
        q.prepare(text);
        q.bindValue(QStringLiteral(":t"), when);
        if (text.contains(QLatin1String(":p"))) {
            q.bindValue(QStringLiteral(":t2"), when);
            q.bindValue(QStringLiteral(":p"), numberPrefix);
        }
        q.bindValue(QStringLiteral(":id"), orderId);
        if (auto st = sql::exec(q, "DemoSeeder::backdateOrder"); !st)
            return st;
    }
    return Status::ok();
}

// ---------------------------------------------------------------- Khata sample data
// Seven regular customers buy on khata; one more has a khata that is open but unused.
struct KhataLimitRow
{
    int customer;
    qint64 limitRupees;
};
const KhataLimitRow kKhataLimits[] = {
    {0, 30000}, {2, 40000}, {4, 10000}, {5, 30000}, {11, 20000}, {18, 15000}, {26, 20000}, {32, 50000},
};

// Indexes into sampleOrders() that are paid on khata instead of Cash on Delivery.
const int kKhataOrders[] = {12, 14, 21, 26, 28, 29, 33, 40, 42, 43, 46, 48, 50, 51, 53, 54, 55, 56, 61, 65, 70};

// Money the customers brought to the shop. rupees = 0 means "everything owed".
// Negative adjustments lower what the customer owes.
struct KhataEventRow
{
    int customer;
    int daysAgo;
    KhataKind kind;
    qint64 rupees;
    const char *note;
};
const KhataEventRow kKhataEvents[] = {
    {2, 9, KhataKind::Adjustment, -393, "Rounded off the bill"},
    {2, 8, KhataKind::Payment, 22000, "Bank transfer"},
    {5, 10, KhataKind::Payment, 5000, "Easypaisa"},
    {0, 7, KhataKind::Payment, 0, "Cash"},
    {18, 5, KhataKind::Payment, 0, "Cash"},
    {5, 3, KhataKind::Payment, 3000, "Cash"},
    {11, 2, KhataKind::Payment, 6499, "JazzCash"},
    {32, 1, KhataKind::Payment, 10000, "Bank transfer"},
};

const QVector<DemoOrder> &sampleOrders()
{
    // About 70 orders in every state, spread over the last three weeks (oldest first)
    static const QVector<DemoOrder> orders = {
        {37, {{"ELEC-1003", 1}}, OrderStatus::Delivered, 20},
        {5, {{"FASH-2001", 1}}, OrderStatus::Delivered, 20},
        {37, {{"BEAU-6002", 2}}, OrderStatus::Cancelled, 19},
        {3, {{"FASH-2001", 1}, {"BEAU-6002", 1}}, OrderStatus::Cancelled, 19},
        {20, {{"BOOK-4002", 2}}, OrderStatus::Delivered, 18},
        {2, {{"BOOK-4003", 1}, {"SPRT-5002", 1}}, OrderStatus::Delivered, 18},
        {17, {{"BEAU-6003", 1}}, OrderStatus::Delivered, 17},
        {21, {{"SPRT-5002", 1}}, OrderStatus::Delivered, 17},
        {5, {{"BOOK-4002", 1}}, OrderStatus::Delivered, 16},
        {10, {{"HOME-3002", 1}, {"HOME-3003", 1}}, OrderStatus::Delivered, 16},
        {38, {{"ELEC-1003", 1}}, OrderStatus::Delivered, 15},
        {37, {{"FASH-2004", 1}}, OrderStatus::Delivered, 15},
        {2, {{"SPRT-5002", 3}, {"ELEC-1005", 1}}, OrderStatus::Delivered, 14},
        {4, {{"BEAU-6002", 1}, {"SPRT-5003", 1}}, OrderStatus::Delivered, 14},
        {5, {{"ELEC-1001", 1}, {"BEAU-6002", 3}}, OrderStatus::Delivered, 14},
        {17, {{"BOOK-4001", 1}}, OrderStatus::Cancelled, 13},
        {15, {{"ELEC-1001", 1}, {"SPRT-5002", 1}}, OrderStatus::Delivered, 13},
        {4, {{"BOOK-4003", 1}, {"ELEC-1005", 1}}, OrderStatus::Delivered, 13},
        {1, {{"SPRT-5004", 1}, {"FASH-2005", 1}}, OrderStatus::Delivered, 12},
        {9, {{"SPRT-5001", 1}, {"BOOK-4002", 1}, {"ELEC-1004", 1}}, OrderStatus::Delivered, 12},
        {12, {{"FASH-2003", 1}}, OrderStatus::Delivered, 12},
        {0, {{"ELEC-1001", 1}, {"BOOK-4002", 2}}, OrderStatus::Delivered, 12},
        {4, {{"HOME-3004", 1}, {"BEAU-6001", 1}}, OrderStatus::Delivered, 11},
        {29, {{"BOOK-4003", 1}, {"BOOK-4002", 3}}, OrderStatus::Delivered, 11},
        {28, {{"HOME-3002", 1}}, OrderStatus::Delivered, 11},
        {4, {{"ELEC-1004", 1}, {"BOOK-4004", 1}}, OrderStatus::Delivered, 10},
        {2, {{"ELEC-1005", 1}, {"BEAU-6001", 1}, {"HOME-3005", 1}}, OrderStatus::Delivered, 10},
        {27, {{"BEAU-6002", 1}}, OrderStatus::Delivered, 10},
        {18, {{"HOME-3005", 1}}, OrderStatus::Delivered, 9},
        {18, {{"BEAU-6002", 1}, {"SPRT-5004", 1}}, OrderStatus::Delivered, 9},
        {12, {{"HOME-3002", 1}, {"FASH-2005", 1}}, OrderStatus::Delivered, 9},
        {7, {{"BOOK-4001", 1}}, OrderStatus::Delivered, 8},
        {36, {{"BOOK-4002", 1}}, OrderStatus::Cancelled, 8},
        {5, {{"FASH-2002", 1}}, OrderStatus::Delivered, 8},
        {1, {{"FASH-2005", 1}}, OrderStatus::Delivered, 7},
        {13, {{"BOOK-4002", 1}, {"HOME-3001", 1}}, OrderStatus::Delivered, 7},
        {10, {{"BEAU-6002", 1}}, OrderStatus::Delivered, 7},
        {19, {{"BEAU-6002", 1}}, OrderStatus::Delivered, 7},
        {31, {{"BOOK-4002", 1}}, OrderStatus::Delivered, 6},
        {5, {{"BEAU-6001", 1}}, OrderStatus::Delivered, 6},
        {26, {{"ELEC-1001", 1}}, OrderStatus::Cancelled, 6},
        {16, {{"ELEC-1005", 1}}, OrderStatus::Delivered, 6},
        {11, {{"BEAU-6002", 1}}, OrderStatus::Shipped, 5},
        {32, {{"ELEC-1003", 1}, {"BOOK-4002", 1}}, OrderStatus::Processing, 5},
        {9, {{"BOOK-4002", 1}, {"HOME-3002", 1}, {"FASH-2002", 1}}, OrderStatus::Shipped, 5},
        {3, {{"SPRT-5003", 1}}, OrderStatus::Cancelled, 5},
        {11, {{"HOME-3001", 1}}, OrderStatus::Delivered, 4},
        {7, {{"BEAU-6002", 1}, {"BEAU-6001", 1}}, OrderStatus::Shipped, 4},
        {32, {{"BEAU-6001", 1}}, OrderStatus::Shipped, 4},
        {11, {{"BOOK-4002", 2}, {"SPRT-5001", 1}}, OrderStatus::Delivered, 4},
        {0, {{"FASH-2002", 2}, {"BEAU-6001", 3}}, OrderStatus::Shipped, 4},
        {32, {{"HOME-3002", 1}}, OrderStatus::Processing, 3},
        {14, {{"BOOK-4002", 2}, {"BOOK-4001", 1}}, OrderStatus::Shipped, 3},
        {26, {{"HOME-3005", 1}, {"BOOK-4002", 1}}, OrderStatus::Delivered, 3},
        {32, {{"BOOK-4001", 3}}, OrderStatus::Shipped, 3},
        {5, {{"BOOK-4002", 2}}, OrderStatus::Processing, 2},
        {18, {{"FASH-2002", 1}}, OrderStatus::Processing, 2},
        {20, {{"SPRT-5002", 2}}, OrderStatus::Confirmed, 2},
        {16, {{"BEAU-6002", 1}}, OrderStatus::Cancelled, 2},
        {0, {{"ELEC-1002", 1}}, OrderStatus::Confirmed, 2},
        {5, {{"BEAU-6001", 1}, {"BOOK-4002", 1}}, OrderStatus::Processing, 1},
        {26, {{"BEAU-6003", 1}, {"HOME-3002", 1}}, OrderStatus::Confirmed, 1},
        {16, {{"FASH-2004", 1}}, OrderStatus::Confirmed, 1},
        {19, {{"SPRT-5003", 1}}, OrderStatus::Pending, 1},
        {15, {{"BEAU-6002", 1}, {"FASH-2003", 1}}, OrderStatus::Confirmed, 1},
        {0, {{"FASH-2005", 1}}, OrderStatus::Cancelled, 1},
        {31, {{"FASH-2004", 1}}, OrderStatus::Pending, 0},
        {35, {{"FASH-2004", 1}, {"BEAU-6002", 1}}, OrderStatus::Pending, 0},
        {5, {{"BEAU-6003", 1}, {"SPRT-5001", 1}}, OrderStatus::Confirmed, 0},
        {4, {{"BEAU-6002", 1}}, OrderStatus::Pending, 0},
        {2, {{"FASH-2003", 1}, {"SPRT-5004", 1}}, OrderStatus::Pending, 0},
        {0, {{"HOME-3002", 1}}, OrderStatus::Pending, 0},
    };
    return orders;
}

// ---------------------------------------------------------------- Sample data version 2: wholesale
// Bulk orders at wholesale prices (Cash on Delivery), placed after the regular ones.
const QVector<DemoOrder> &bulkOrders()
{
    static const QVector<DemoOrder> orders = {
        {40, {{"SPRT-5004", 12}}, OrderStatus::Confirmed, 0},                     // a Sialkot football club
        {31, {{"BEAU-6002", 6}, {"BEAU-6001", 12}}, OrderStatus::Processing, 0}, // a Karachi salon
        {23, {{"BOOK-4002", 10}}, OrderStatus::Pending, 0},                       // a school book club
    };
    return orders;
}

// ---------------------------------------------------------------- Sample data version 3: qist
struct DemoPlan
{
    const char *name;
    int months;
    int markupBp;
    int minDownBp;
};
const DemoPlan kPlans[] = {
    {"3 months, no markup", 3, 0, 2500},
    {"6 months", 6, 1000, 2000},
    {"12 months", 12, 2000, 2000},
};

// Phones, gadgets, appliances and sports gear: what Pakistani shops sell on qist.
const char *const kQistSkus[] = {
    "ELEC-1001", "ELEC-1002", "ELEC-1003", "ELEC-1004", "ELEC-1006",
    "HOME-3001", "HOME-3003", "HOME-3004", "SPRT-5001", "SPRT-5003",
};

// Customers the shop allows qist: Ayesha (the account hinted on the sign-in
// screen, so qist can be tried at checkout) and the five below. None of the
// five has a khata, so the khata figures in docs/CUSTOMER_ACCOUNTS.md stay the same.
const int kQistCustomers[] = {0, 6, 14, 21, 30, 33};

struct DemoQist
{
    int customer;
    const char *sku;
    const char *plan;
    int daysAgo;               // when the order was placed
    OrderStatus finalStatus;
    QVector<int> paidDaysAgo;  // one installment paid on each of these days (the first is the down payment)
};

const QVector<DemoQist> &sampleQists()
{
    static const QVector<DemoQist> qists = {
        {14, "ELEC-1002", "3 months, no markup", 100, OrderStatus::Delivered, {98, 70, 40, 10}}, // Ali: paid off
        {6, "HOME-3004", "6 months", 75, OrderStatus::Delivered, {73, 44, 12}},                // Bilal: on time
        {21, "HOME-3001", "6 months", 70, OrderStatus::Delivered, {68, 38}},                   // Mahnoor: one overdue
        {30, "SPRT-5001", "12 months", 25, OrderStatus::Delivered, {23}},                      // Junaid: just started
        {33, "ELEC-1004", "6 months", 0, OrderStatus::Pending, {}},                            // Areeba: down payment due
    };
    return qists;
}

const char *const kPaidBy[] = {"Cash", "Easypaisa", "JazzCash", "Bank transfer"};

// ---------------------------------------------------------------- helpers for seed() and upgrade()

QSqlDatabase dbOf(AppContext &ctx)
{
    return QSqlDatabase::database(ctx.connectionName(), false);
}

QHash<QString, qint64> productIdsBySku(AppContext &ctx)
{
    QHash<QString, qint64> ids;
    QSqlQuery q(dbOf(ctx));
    if (q.exec(QStringLiteral("SELECT sku, id FROM products"))) {
        while (q.next())
            ids.insert(q.value(0).toString(), q.value(1).toLongLong());
    }
    return ids;
}

// A sample customer as a session. Sample data is created inside the app on the
// admin's behalf, so it never needs the customer's password.
std::optional<Session> sampleCustomer(AppContext &ctx, const QString &email)
{
    QSqlQuery q(dbOf(ctx));
    q.prepare(QStringLiteral(
        "SELECT id, full_name, email FROM users WHERE email = :e AND role = 'customer' AND is_active = 1"));
    q.bindValue(QStringLiteral(":e"), email);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return Session(q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString(), Role::Customer);
}

bool hasOrderLine(AppContext &ctx, qint64 userId, const QString &sku, int quantity)
{
    QSqlQuery q(dbOf(ctx));
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM order_items oi JOIN orders o ON o.id = oi.order_id "
                             "WHERE o.user_id = :u AND oi.sku = :s AND oi.quantity = :n"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":s"), sku);
    q.bindValue(QStringLiteral(":n"), quantity);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool hasQist(AppContext &ctx, qint64 userId)
{
    QSqlQuery q(dbOf(ctx));
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM qist_accounts WHERE user_id = :u"));
    q.bindValue(QStringLiteral(":u"), userId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool cartIsEmpty(AppContext &ctx, const Session &customer)
{
    auto count = ctx.cart().itemCount(customer);
    return count && count.value() == 0;
}

// Places one sample order the way a customer would (cart, checkout), then the
// admin moves it to its final status, or the customer cancels it. The order is
// dated `timeIndex`-th in the sample period. Returns the new order id.
Result<qint64> placeSampleOrder(AppContext &ctx, const Session &admin, const Session &customer,
                                const DemoSeeder::SampleAccount &info, const DemoOrder &o, int timeIndex,
                                const QHash<QString, qint64> &productIds, const QString &paymentMethod)
{
    const QDateTime placedAt = sampleTime(o.daysAgo, timeIndex);
    sql::ScopedFixedClock clock(placedAt);
    for (const auto &item : o.items) {
        const qint64 productId = productIds.value(QString::fromLatin1(item.first));
        if (productId == 0)
            return Result<qint64>::fail(QStringLiteral("Sample product %1 is missing.").arg(QString::fromLatin1(item.first)));
        if (auto st = ctx.cart().add(customer, productId, item.second); !st)
            return st;
    }
    CheckoutRequest req;
    req.checkoutToken = CheckoutService::newCheckoutToken();
    req.shipping = {info.fullName, info.phone, info.address, info.city};
    req.paymentMethod = paymentMethod;
    auto placed = ctx.checkout().placeOrder(customer, req);
    if (!placed)
        return placed.status();
    const qint64 orderId = placed.value().id;

    if (o.finalStatus == OrderStatus::Cancelled) {
        const QString reason = QString::fromLatin1(kCancelReasons[timeIndex % std::size(kCancelReasons)]);
        if (auto st = ctx.orders().cancel(customer, orderId, reason); !st)
            return st;
    } else {
        OrderStatus current = OrderStatus::Pending;
        while (current != o.finalStatus) {
            const OrderStatus next = *orderstatus::nextForward(current);
            if (auto st = ctx.orders().advance(admin, orderId, next); !st)
                return st;
            current = next;
        }
    }
    if (auto st = backdateOrder(ctx.connectionName(), orderId, placedAt); !st)
        return st;
    return orderId;
}

// Places one sample qist: the order, the admin moving it along, and the
// installments paid on the days listed (the first one is the down payment;
// the order is delivered once it is in hand).
Status placeSampleQist(AppContext &ctx, const Session &admin, const Session &customer,
                       const DemoSeeder::SampleAccount &info, const DemoQist &d, int timeIndex,
                       const QHash<QString, qint64> &productIds, qint64 planId)
{
    const qint64 productId = productIds.value(QString::fromLatin1(d.sku));
    if (productId == 0 || planId == 0)
        return Status::fail(QStringLiteral("Sample qist product or plan %1 is missing.").arg(QString::fromLatin1(d.sku)));

    // A qist bought "today" is placed right now, so its down payment is due today
    // even when the sample data is loaded just after midnight.
    const QDateTime placedAt = d.daysAgo == 0 ? QDateTime::currentDateTimeUtc() : sampleTime(d.daysAgo, timeIndex);
    qint64 orderId = 0;
    qint64 qistId = 0;
    {
        sql::ScopedFixedClock clock(placedAt);
        if (auto st = ctx.cart().add(customer, productId, 1); !st)
            return st;
        CheckoutRequest req;
        req.checkoutToken = CheckoutService::newCheckoutToken();
        req.shipping = {info.fullName, info.phone, info.address, info.city};
        req.paymentMethod = payment::kQist;
        req.qistPlanId = planId;
        auto placed = ctx.checkout().placeOrder(customer, req);
        if (!placed)
            return placed.status();
        orderId = placed.value().id;
        auto created = ctx.qist().forOrder(admin, orderId);
        if (!created)
            return created.status();
        if (!created.value())
            return Status::fail(QStringLiteral("The sample qist was not created."));
        qistId = created.value()->id;
        if (d.finalStatus != OrderStatus::Pending) {
            for (OrderStatus step : {OrderStatus::Confirmed, OrderStatus::Processing, OrderStatus::Shipped}) {
                if (auto st = ctx.orders().advance(admin, orderId, step); !st)
                    return st;
            }
        }
    }

    for (int k = 0; k < d.paidDaysAgo.size(); ++k) {
        sql::ScopedFixedClock clock(QDateTime::currentDateTimeUtc().addDays(-d.paidDaysAgo[k]).addSecs(-2 * 3600));
        auto account = ctx.qist().account(admin, qistId);
        if (!account)
            return account.status();
        const std::optional<InstallmentStatus> next = account.value().nextDue();
        if (!next)
            break;
        const QString paidBy = QString::fromLatin1(kPaidBy[(timeIndex + k) % int(std::size(kPaidBy))]);
        if (auto paid = ctx.qist().recordPayment(admin, qistId, next->remaining(), paidBy); !paid)
            return paid.status();
        if (k == 0 && d.finalStatus == OrderStatus::Delivered) {
            if (auto st = ctx.orders().advance(admin, orderId, OrderStatus::Delivered); !st)
                return st;
        }
    }
    return backdateOrder(ctx.connectionName(), orderId, placedAt);
}

// Sample data version 2: wholesale tiers on 6 products and 3 bulk orders that use them.
// strict = new sample data, where every step must work. Otherwise (bringing older
// sample data up to date) anything that no longer fits is skipped: a product or
// customer the shop deleted, tiers the shop already set, an order already there,
// a sample customer with something in their cart, too little stock.
Status addWholesale(AppContext &ctx, const Session &admin, bool strict)
{
    const QHash<QString, qint64> productIds = productIdsBySku(ctx);

    QHash<QString, QVector<PriceTier>> tiersBySku;
    for (const DemoTier &t : kTiers) {
        PriceTier tier;
        tier.minQuantity = t.minQuantity;
        tier.discountBp = t.discountBp;
        tiersBySku[QString::fromLatin1(t.sku)].push_back(tier);
    }
    for (auto it = tiersBySku.cbegin(); it != tiersBySku.cend(); ++it) {
        const qint64 productId = productIds.value(it.key());
        if (productId == 0) {
            if (strict)
                return Status::fail(QStringLiteral("Sample product %1 is missing.").arg(it.key()));
            continue;
        }
        auto existing = ctx.catalog().priceTiers(admin, productId);
        if (!existing)
            return existing.status();
        if (!existing.value().isEmpty())
            continue; // keep the quantity prices the shop already has
        if (auto st = ctx.catalog().setPriceTiers(admin, productId, it.value()); !st)
            return st;
    }

    const QVector<DemoSeeder::SampleAccount> accounts = DemoSeeder::sampleCustomers();
    const QVector<DemoOrder> &orders = bulkOrders();
    for (int i = 0; i < orders.size(); ++i) {
        const DemoOrder &o = orders[i];
        const DemoSeeder::SampleAccount &info = accounts[o.customer];
        const std::optional<Session> customer = sampleCustomer(ctx, info.email);
        if (!customer) {
            if (strict)
                return Status::fail(QStringLiteral("Sample customer %1 is missing.").arg(info.email));
            continue;
        }
        const auto &first = o.items.first();
        if (hasOrderLine(ctx, customer->userId(), QString::fromLatin1(first.first), first.second))
            continue; // already placed by an earlier version of the sample data
        if (!cartIsEmpty(ctx, *customer)) {
            if (strict)
                return Status::fail(QStringLiteral("Sample customer %1 has a cart.").arg(info.email));
            continue; // never touch a cart someone is filling
        }
        auto placed = placeSampleOrder(ctx, admin, *customer, info, o, int(sampleOrders().size()) + i, productIds,
                                       payment::kCashOnDelivery);
        if (!placed) {
            if (strict)
                return placed.status();
            qWarning().noquote() << "Sample bulk order skipped:" << placed.error();
            ctx.cart().clear(*customer); // it was empty before
        }
    }
    return Status::ok();
}

// Sample data version 3: qist plans, products and customers, and 5 qists in
// every state (paid off, on time, overdue, just started, new). Same rules for
// `strict` as addWholesale().
Status addQist(AppContext &ctx, const Session &admin, bool strict)
{
    QHash<QString, qint64> planIds; // by lower-case name
    auto plans = ctx.qist().plans(admin, true);
    if (!plans)
        return plans.status();
    for (const QistPlan &p : plans.value())
        planIds.insert(p.name.toLower(), p.id);
    for (const DemoPlan &p : kPlans) {
        const QString name = QString::fromUtf8(p.name);
        if (planIds.contains(name.toLower()))
            continue; // the shop already has a plan with this name
        auto id = ctx.qist().createPlan(admin, QistPlanInput{name, QistTerms{p.months, p.markupBp, p.minDownBp}});
        if (!id)
            return id.status();
        planIds.insert(name.toLower(), id.value());
    }

    const QHash<QString, qint64> productIds = productIdsBySku(ctx);
    for (const char *sku : kQistSkus) {
        const qint64 productId = productIds.value(QString::fromLatin1(sku));
        if (productId == 0) {
            if (strict)
                return Status::fail(QStringLiteral("Sample product %1 is missing.").arg(QString::fromLatin1(sku)));
            continue;
        }
        if (auto st = ctx.catalog().setQistAllowed(admin, productId, true); !st)
            return st;
    }

    const QVector<DemoSeeder::SampleAccount> accounts = DemoSeeder::sampleCustomers();
    for (int c : kQistCustomers) {
        const std::optional<Session> customer = sampleCustomer(ctx, accounts[c].email);
        if (!customer) {
            if (strict)
                return Status::fail(QStringLiteral("Sample customer %1 is missing.").arg(accounts[c].email));
            continue;
        }
        if (auto st = ctx.qist().setApproved(admin, customer->userId(), true); !st)
            return st;
    }

    const QVector<DemoQist> &qists = sampleQists();
    for (int i = 0; i < qists.size(); ++i) {
        const DemoQist &d = qists[i];
        const DemoSeeder::SampleAccount &info = accounts[d.customer];
        const std::optional<Session> customer = sampleCustomer(ctx, info.email);
        if (!customer)
            continue; // reported above
        if (hasQist(ctx, customer->userId()))
            continue; // this customer already has one
        if (!cartIsEmpty(ctx, *customer)) {
            if (strict)
                return Status::fail(QStringLiteral("Sample customer %1 has a cart.").arg(info.email));
            continue;
        }
        const Status st = placeSampleQist(ctx, admin, *customer, info, d, 90 + i, productIds,
                                          planIds.value(QString::fromUtf8(d.plan).toLower()));
        if (!st) {
            if (strict)
                return st;
            qWarning().noquote() << "Sample qist skipped:" << st.error();
            ctx.cart().clear(*customer); // it was empty before
        }
    }
    return Status::ok();
}

Status setDataVersion(AppContext &ctx, int version)
{
    return ctx.settings().set(QStringLiteral("demo_data_version"), QString::number(version));
}


} // namespace

QVector<DemoSeeder::SampleAccount> DemoSeeder::sampleCustomers()
{
    QVector<SampleAccount> out;
    for (const SampleCustomerRow &c : kCustomers) {
        const QString first = QString::fromUtf8(c.firstName);
        const QString last = QString::fromUtf8(c.lastName);
        out.push_back({first + QLatin1Char(' ') + last,
                       QStringLiteral("%1.%2@example.com").arg(first.toLower(), last.toLower()),
                       first + QStringLiteral("123"),
                       QString::fromLatin1(c.phone),
                       QString::fromUtf8(c.address),
                       QString::fromUtf8(c.city)});
    }
    return out;
}

int DemoSeeder::sampleOrderCount()
{
    return int(sampleOrders().size() + bulkOrders().size() + sampleQists().size());
}

int DemoSeeder::sampleKhataCustomerCount()
{
    return int(std::size(kKhataLimits));
}

int DemoSeeder::sampleWholesaleProductCount()
{
    QSet<QString> skus;
    for (const DemoTier &t : kTiers)
        skus.insert(QString::fromLatin1(t.sku));
    return int(skus.size());
}

int DemoSeeder::sampleQistPlanCount()
{
    return int(std::size(kPlans));
}

int DemoSeeder::sampleQistProductCount()
{
    return int(std::size(kQistSkus));
}

int DemoSeeder::sampleQistCustomerCount()
{
    return int(std::size(kQistCustomers));
}

int DemoSeeder::sampleQistCount()
{
    return int(sampleQists().size());
}

bool DemoSeeder::isLoaded(AppContext &ctx)
{
    auto v = ctx.settings().get(QStringLiteral("demo_data_loaded"), QStringLiteral("0"));
    return v && v.value() == QLatin1String("1");
}

int DemoSeeder::dataVersion(AppContext &ctx)
{
    if (!isLoaded(ctx))
        return 0;
    // Sample data from before versions were recorded (ShopHub v2 milestone 2 and older) counts as 1.
    auto v = ctx.settings().get(QStringLiteral("demo_data_version"), QStringLiteral("1"));
    bool ok = false;
    const int version = v ? v.value().toInt(&ok) : 1;
    return ok && version >= 1 ? version : 1;
}

bool DemoSeeder::needsUpgrade(AppContext &ctx)
{
    const int version = dataVersion(ctx);
    return version > 0 && version < kDataVersion;
}

Status DemoSeeder::upgrade(AppContext &ctx, const Session &admin)
{
    if (!admin.isAdmin())
        return Status::fail(QStringLiteral("You do not have permission to do this."));
    const int version = dataVersion(ctx);
    if (version == 0 || version >= kDataVersion)
        return Status::ok(); // no sample data (a real shop), or already up to date

    qInfo() << "Updating sample data from version" << version << "to" << kDataVersion;
    if (version < 2) {
        if (auto st = addWholesale(ctx, admin, false); !st)
            return st;
        if (auto st = setDataVersion(ctx, 2); !st)
            return st;
    }
    if (version < 3) {
        if (auto st = addQist(ctx, admin, false); !st)
            return st;
        if (auto st = setDataVersion(ctx, 3); !st)
            return st;
    }
    qInfo() << "Sample data is now version" << kDataVersion;
    return Status::ok();
}

Status DemoSeeder::seed(AppContext &ctx, const Session &admin, int version)
{
    if (!admin.isAdmin())
        return Status::fail(QStringLiteral("You do not have permission to do this."));
    if (isLoaded(ctx))
        return Status::fail(QStringLiteral("Demo data has already been loaded."));
    version = qBound(1, version, kDataVersion);

    qInfo() << "Loading sample data";

    // Categories
    QHash<QString, qint64> categoryIds;
    for (const DemoProduct &p : kProducts) {
        const QString cat = QString::fromUtf8(p.category);
        if (categoryIds.contains(cat))
            continue;
        auto id = ctx.catalog().createCategory(admin, cat);
        if (!id)
            return id.status();
        categoryIds.insert(cat, id.value());
    }

    const QVector<DemoOrder> &orders = sampleOrders();

    // Stock every sample order will use up, so final stock matches the table above.
    QHash<QString, int> consumed;
    for (const QVector<DemoOrder> *list : {&orders, &bulkOrders()}) {
        for (const DemoOrder &o : *list) {
            if (o.finalStatus == OrderStatus::Cancelled)
                continue;
            for (const auto &item : o.items)
                consumed[QString::fromLatin1(item.first)] += item.second;
        }
    }
    for (const DemoQist &q : sampleQists())
        consumed[QString::fromLatin1(q.sku)] += 1;

    // Products
    QHash<QString, qint64> productIds;
    for (const DemoProduct &p : kProducts) {
        ProductInput in;
        in.sku = QString::fromLatin1(p.sku);
        in.name = QString::fromUtf8(p.name);
        in.brand = QString::fromUtf8(p.brand);
        in.description = QString::fromUtf8(p.description);
        in.categoryId = categoryIds.value(QString::fromUtf8(p.category));
        in.price = Money::fromRupees(p.priceRupees);
        in.cost = Money::fromRupees(p.costRupees);
        in.initialStock = p.stock + consumed.value(in.sku);
        in.lowStockThreshold = 5;
        auto id = ctx.catalog().createProduct(admin, in);
        if (!id)
            return id.status();
        productIds.insert(in.sku, id.value());
    }

    // Sample customers (fast hashing; upgraded to full strength on first sign-in)
    const QVector<SampleAccount> accounts = sampleCustomers();
    QVector<Session> customers;
    for (const SampleAccount &a : accounts) {
        auto session = ctx.auth().createSampleCustomer(admin, a.fullName, a.email, a.password);
        if (!session)
            return session.status();
        customers.push_back(session.value());
    }

    // Khata: credit limits from the start of the sample period.
    {
        sql::ScopedFixedClock clock(sampleTime(21, 0));
        for (const KhataLimitRow &k : kKhataLimits) {
            if (auto st = ctx.khata().setLimit(admin, customers[k.customer].userId(), Money::fromRupees(k.limitRupees)); !st)
                return st;
        }
    }
    const QSet<int> khataOrders(std::begin(kKhataOrders), std::end(kKhataOrders));
    int nextEvent = 0;
    auto runKhataEventsBefore = [&](int daysAgo) -> Status {
        // Every payment or adjustment that happened before an order placed `daysAgo` days ago.
        while (nextEvent < int(std::size(kKhataEvents)) && kKhataEvents[nextEvent].daysAgo > daysAgo) {
            const KhataEventRow &e = kKhataEvents[nextEvent++];
            sql::ScopedFixedClock clock(QDateTime::currentDateTimeUtc().addDays(-e.daysAgo).addSecs(-3600));
            const qint64 userId = customers[e.customer].userId();
            if (e.kind == KhataKind::Payment) {
                Money amount = Money::fromRupees(e.rupees);
                if (e.rupees == 0) {
                    auto account = ctx.khata().account(admin, userId);
                    if (!account)
                        return account.status();
                    amount = account.value().creditUsed();
                }
                if (auto r = ctx.khata().recordPayment(admin, userId, amount, QString::fromLatin1(e.note)); !r)
                    return r.status();
            } else {
                if (auto r = ctx.khata().adjust(admin, userId, Money::fromRupees(e.rupees), QString::fromLatin1(e.note)); !r)
                    return r.status();
            }
        }
        return Status::ok();
    };

    for (int i = 0; i < orders.size(); ++i) {
        const DemoOrder &o = orders[i];
        if (auto st = runKhataEventsBefore(o.daysAgo); !st)
            return st;
        auto placed = placeSampleOrder(ctx, admin, customers[o.customer], accounts[o.customer], o, i, productIds,
                                       khataOrders.contains(i) ? payment::kKhata : payment::kCashOnDelivery);
        if (!placed)
            return placed.status();
    }
    if (auto st = runKhataEventsBefore(-1); !st)
        return st;

    if (auto st = ctx.settings().set(QStringLiteral("demo_data_loaded"), QStringLiteral("1")); !st)
        return st;
    if (auto st = setDataVersion(ctx, 1); !st)
        return st;

    // Later versions of the sample data, the same steps upgrade() runs on older databases.
    if (version >= 2) {
        if (auto st = addWholesale(ctx, admin, true); !st)
            return st;
        if (auto st = setDataVersion(ctx, 2); !st)
            return st;
    }
    if (version >= 3) {
        if (auto st = addQist(ctx, admin, true); !st)
            return st;
        if (auto st = setDataVersion(ctx, 3); !st)
            return st;
    }
    qInfo() << "Sample data loaded:" << customers.size() << "customers," << sampleOrderCount() << "orders";
    return Status::ok();
}

} // namespace shophub
