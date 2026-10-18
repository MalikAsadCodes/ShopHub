#include "TestSupport.h"

using namespace shophub;

class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_b = std::make_unique<test::Backend>();
        QVERIFY(m_b->init());
    }
    void cleanup() { m_b.reset(); }

    void customerCannotManageCatalog()
    {
        auto &catalog = m_b->ctx->catalog();
        ProductInput in;
        in.sku = QStringLiteral("HACK-1");
        in.name = QStringLiteral("Hack");
        in.categoryId = m_b->categoryId;
        in.price = Money::fromRupees(1);
        QVERIFY(!catalog.createProduct(m_b->customer, in).isOk());
        QVERIFY(!catalog.createCategory(m_b->customer, QStringLiteral("Hack")).isOk());
        QVERIFY(!catalog.setProductActive(m_b->customer, 1, false).isOk());
        QVERIFY(!catalog.createProduct(Session(), in).isOk()); // not signed in
    }

    void productValidation()
    {
        auto &catalog = m_b->ctx->catalog();
        ProductInput in;
        in.sku = QStringLiteral("val-1"); // lower case is normalised to VAL-1
        in.name = QStringLiteral("Valid");
        in.categoryId = m_b->categoryId;
        in.price = Money::fromRupees(10);
        QVERIFY(catalog.createProduct(m_b->admin, in).isOk());

        QVERIFY2(!catalog.createProduct(m_b->admin, in).isOk(), "duplicate SKU must fail");

        ProductInput bad = in;
        bad.sku = QStringLiteral("VAL-2");
        bad.price = Money();
        QVERIFY(!catalog.createProduct(m_b->admin, bad).isOk());
        bad.price = Money::fromRupees(5);
        bad.categoryId = 424242;
        QVERIFY(!catalog.createProduct(m_b->admin, bad).isOk());
        bad.categoryId = m_b->categoryId;
        bad.initialStock = -1;
        QVERIFY(!catalog.createProduct(m_b->admin, bad).isOk());
    }

    void initialStockIsRecordedAsMovement()
    {
        const qint64 id = m_b->addProduct(QStringLiteral("MOV-1"), 100, 7);
        auto moves = m_b->ctx->inventory().movements(m_b->admin, id);
        QVERIFY(moves.isOk());
        QCOMPARE(moves.value().size(), 1);
        QCOMPARE(moves.value().first().change, 7);
        QCOMPARE(moves.value().first().reason, StockReason::Initial);
    }

    void costPriceHiddenFromCustomers()
    {
        const qint64 id = m_b->addProduct(QStringLiteral("COST-1"), 1000, 3);
        QVERIFY(m_b->ctx->catalog().product(m_b->admin, id).value().cost.has_value());
        QVERIFY(!m_b->ctx->catalog().product(m_b->customer, id).value().cost.has_value());
        auto page = m_b->ctx->catalog().search(m_b->customer, ProductFilter());
        for (const Product &p : page.value().items)
            QVERIFY(!p.cost.has_value());
    }

    void searchAndFilters()
    {
        m_b->addProduct(QStringLiteral("SRC-1"), 500, 10, QStringLiteral("Blue Kettle"));
        m_b->addProduct(QStringLiteral("SRC-2"), 1500, 0, QStringLiteral("Red Kettle"));
        m_b->addProduct(QStringLiteral("SRC-3"), 2500, 5, QStringLiteral("Green Lamp"));
        auto &catalog = m_b->ctx->catalog();

        ProductFilter f;
        f.text = QStringLiteral("kettle");
        QCOMPARE(catalog.search(m_b->customer, f).value().total, 2);

        f.inStockOnly = true;
        QCOMPARE(catalog.search(m_b->customer, f).value().total, 1);

        ProductFilter price;
        price.minPrice = Money::fromRupees(1000);
        price.maxPrice = Money::fromRupees(2000);
        QCOMPARE(catalog.search(m_b->customer, price).value().total, 1);

        ProductFilter sku;
        sku.text = QStringLiteral("SRC-3");
        QCOMPARE(catalog.search(m_b->customer, sku).value().items.first().name, QStringLiteral("Green Lamp"));

        ProductFilter sorted;
        sorted.sort = ProductSort::PriceHighToLow;
        QCOMPARE(catalog.search(m_b->customer, sorted).value().items.first().name, QStringLiteral("Green Lamp"));

        ProductFilter paged;
        paged.sort = ProductSort::PriceLowToHigh;
        paged.limit = 2;
        paged.offset = 2;
        auto page = catalog.search(m_b->customer, paged).value();
        QCOMPARE(page.total, 3);
        QCOMPARE(page.items.size(), 1);
        QCOMPARE(page.items.first().name, QStringLiteral("Green Lamp"));
    }

    void wildcardsAreMatchedLiterally()
    {
        m_b->addProduct(QStringLiteral("WILD-1"), 100, 1, QStringLiteral("Plain Mug"));
        ProductFilter f;
        f.text = QStringLiteral("%");
        QCOMPARE(m_b->ctx->catalog().search(m_b->customer, f).value().total, 0);
        f.text = QStringLiteral("_");
        QCOMPARE(m_b->ctx->catalog().search(m_b->customer, f).value().total, 0);
    }

    void inactiveProductsHiddenFromCustomers()
    {
        const qint64 id = m_b->addProduct(QStringLiteral("OFF-1"), 100, 5);
        QVERIFY(m_b->ctx->catalog().setProductActive(m_b->admin, id, false).isOk());

        QCOMPARE(m_b->ctx->catalog().search(m_b->customer, ProductFilter()).value().total, 0);
        QVERIFY(!m_b->ctx->catalog().product(m_b->customer, id).isOk());

        ProductFilter all;
        all.includeInactive = true;
        QCOMPARE(m_b->ctx->catalog().search(m_b->admin, all).value().total, 1);
        // A customer asking for inactive products still gets none.
        QCOMPARE(m_b->ctx->catalog().search(m_b->customer, all).value().total, 0);
    }

    void deactivatingCategoryHidesItsProducts()
    {
        m_b->addProduct(QStringLiteral("CAT-1"), 100, 5);
        QVERIFY(m_b->ctx->catalog().setCategoryActive(m_b->admin, m_b->categoryId, false).isOk());
        QCOMPARE(m_b->ctx->catalog().search(m_b->customer, ProductFilter()).value().total, 0);
    }

    void duplicateCategoryRejected()
    {
        auto again = m_b->ctx->catalog().createCategory(m_b->admin, QStringLiteral("electronics"));
        QVERIFY(!again.isOk());
    }

    void imageImportValidatesContent()
    {
        QTemporaryDir dir;
        const QString fake = dir.filePath(QStringLiteral("fake.png"));
        QFile f(fake);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not an image");
        f.close();
        QVERIFY(!m_b->ctx->catalog().importImage(m_b->admin, fake).isOk());

        const QString real = dir.filePath(QStringLiteral("real.png"));
        QFile r(real);
        QVERIFY(r.open(QIODevice::WriteOnly));
        r.write(QByteArray("\x89PNG\r\n\x1a\n", 8) + QByteArray(32, '\0'));
        r.close();
        auto stored = m_b->ctx->catalog().importImage(m_b->admin, real);
        QVERIFY2(stored.isOk(), qPrintable(stored.error()));
        QVERIFY(stored.value().endsWith(QStringLiteral(".png")));
        QVERIFY(!m_b->ctx->images().resolve(stored.value()).isEmpty());
        QVERIFY(m_b->ctx->images().resolve(QStringLiteral("../etc/passwd")).isEmpty());
        QVERIFY(!m_b->ctx->catalog().importImage(m_b->customer, real).isOk());
    }

private:
    std::unique_ptr<test::Backend> m_b;
};

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
