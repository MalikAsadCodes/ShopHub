#include "ui/dialogs/AdminDialogs.h"

#include "services/AppContext.h"
#include "ui/widgets/ImageView.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <utility>

namespace shophub::ui {

namespace {

QLineEdit *lineEdit(const QString &placeholder)
{
    auto *e = new QLineEdit();
    e->setPlaceholderText(placeholder);
    e->setAccessibleName(placeholder);
    e->setMinimumHeight(34);
    return e;
}

} // namespace

// ====================================================================== Product edit
ProductEditDialog::ProductEditDialog(UiContext &ui, std::optional<qint64> productId, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_productId(productId)
{
    setWindowTitle(productId ? QStringLiteral("Edit product") : QStringLiteral("New product"));
    setMinimumSize(760, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    root->addWidget(makeLabel(productId ? QStringLiteral("Edit product") : QStringLiteral("New product"), "h1"));

    auto *columns = new QHBoxLayout();
    columns->setSpacing(24);
    root->addLayout(columns, 1);

    // Form
    m_sku = lineEdit(QStringLiteral("e.g. ELEC-1001"));
    m_name = lineEdit(QStringLiteral("Product name"));
    m_category = new QComboBox();
    m_category->setMinimumHeight(34);
    m_brand = lineEdit(QStringLiteral("Brand (optional)"));
    m_price = lineEdit(QStringLiteral("e.g. 4999 or 4999.50"));
    m_cost = lineEdit(QStringLiteral("Optional - only admins see it"));
    m_stock = new QSpinBox();
    m_stock->setRange(0, 1000000);
    m_stock->setMinimumHeight(34);
    m_lowStock = new QSpinBox();
    m_lowStock->setRange(0, 1000000);
    m_lowStock->setValue(5);
    m_lowStock->setMinimumHeight(34);
    m_description = new QPlainTextEdit();
    m_description->setPlaceholderText(QStringLiteral("What should customers know about this product?"));
    m_description->setMinimumHeight(110);

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->addRow(makeFormLabel(QStringLiteral("SKU")), m_sku);
    form->addRow(makeFormLabel(QStringLiteral("Name")), m_name);
    form->addRow(makeFormLabel(QStringLiteral("Category")), m_category);
    form->addRow(makeFormLabel(QStringLiteral("Brand")), m_brand);
    form->addRow(makeFormLabel(QStringLiteral("Price (Rs)")), m_price);
    form->addRow(makeFormLabel(QStringLiteral("Cost (Rs)")), m_cost);
    form->addRow(makeFormLabel(productId ? QStringLiteral("Stock") : QStringLiteral("Opening stock")), m_stock);
    form->addRow(makeFormLabel(QStringLiteral("Low-stock alert at")), m_lowStock);
    form->addRow(makeFormLabel(QStringLiteral("Description")), m_description);
    columns->addLayout(form, 3);

    // Image
    auto *imageCol = new QVBoxLayout();
    imageCol->setSpacing(8);
    imageCol->addWidget(makeLabel(QStringLiteral("Product image"), "h3"));
    m_preview = new ImageView();
    m_preview->setFixedSize(230, 200);
    imageCol->addWidget(m_preview);
    auto *choose = makeButton(QStringLiteral("Choose image..."));
    auto *remove = makeButton(QStringLiteral("Remove image"), "ghost");
    connect(choose, &QPushButton::clicked, this, &ProductEditDialog::chooseImage);
    connect(remove, &QPushButton::clicked, this, [this] {
        m_imageFile.clear();
        updatePreview();
    });
    imageCol->addWidget(choose);
    imageCol->addWidget(remove);
    imageCol->addWidget(makeLabel(QStringLiteral("PNG, JPEG, WebP, GIF or BMP, up to 5 MB.\n"
                                                 "A copy is stored in ShopHub's data folder."),
                                  "small"));
    imageCol->addStretch();
    columns->addLayout(imageCol, 2);

    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    root->addWidget(m_error);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = makeButton(QStringLiteral("Cancel"), "ghost");
    auto *save = makeButton(productId ? QStringLiteral("Save changes") : QStringLiteral("Create product"), "primary");
    save->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &ProductEditDialog::save);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    connect(m_name, &QLineEdit::textChanged, this, &ProductEditDialog::updatePreview);
    connect(m_category, &QComboBox::currentTextChanged, this, &ProductEditDialog::updatePreview);

    qint64 categoryId = 0;
    if (productId) {
        auto p = ui.app.catalog().product(ui.session, *productId);
        if (!p) {
            showError(parent, p.error());
            m_loaded = false;
            return;
        }
        const Product &prod = p.value();
        m_sku->setText(prod.sku);
        m_name->setText(prod.name);
        m_brand->setText(prod.brand);
        m_price->setText(prod.price.toPlainString());
        if (prod.cost)
            m_cost->setText(prod.cost->toPlainString());
        m_stock->setValue(prod.stock);
        m_stock->setEnabled(false);
        m_stock->setToolTip(QStringLiteral("Use the Inventory page to change stock, so every change is recorded."));
        m_lowStock->setValue(prod.lowStockThreshold);
        m_description->setPlainText(prod.description);
        m_imageFile = prod.imageFile;
        categoryId = prod.categoryId;
    }
    loadCategories(categoryId);
    updatePreview();
}

void ProductEditDialog::loadCategories(qint64 selectedId)
{
    m_category->clear();
    auto cats = m_ui.app.catalog().categories(m_ui.session, true);
    if (!cats)
        return;
    for (const Category &c : cats.value()) {
        m_category->addItem(c.active ? c.name : c.name + QStringLiteral(" (inactive)"), c.id);
        if (c.id == selectedId)
            m_category->setCurrentIndex(m_category->count() - 1);
    }
    if (m_category->count() == 0)
        m_error->setText(QStringLiteral("Create a category first (Products page > Categories)."));
}

void ProductEditDialog::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose product image"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp)"));
    if (path.isEmpty())
        return;
    auto stored = m_ui.app.catalog().importImage(m_ui.session, path);
    if (!stored) {
        m_error->setText(stored.error());
        return;
    }
    m_imageFile = stored.value();
    m_error->clear();
    updatePreview();
}

void ProductEditDialog::updatePreview()
{
    m_preview->setImage(m_ui.app.images().resolve(m_imageFile),
                        m_name->text().isEmpty() ? QStringLiteral("New Product") : m_name->text(),
                        m_category->currentText());
}

void ProductEditDialog::save()
{
    ProductInput in;
    in.sku = m_sku->text();
    in.name = m_name->text();
    in.categoryId = m_category->currentData().toLongLong();
    in.brand = m_brand->text();
    in.description = m_description->toPlainText();
    in.initialStock = m_stock->value();
    in.lowStockThreshold = m_lowStock->value();
    in.imageFile = m_imageFile;

    const auto price = Money::parse(m_price->text());
    if (!price) {
        m_error->setText(QStringLiteral("Enter a valid price, e.g. 4999 or 4999.50."));
        m_price->setFocus();
        return;
    }
    in.price = *price;
    if (!m_cost->text().trimmed().isEmpty()) {
        const auto cost = Money::parse(m_cost->text());
        if (!cost) {
            m_error->setText(QStringLiteral("Enter a valid cost price, or leave it empty."));
            m_cost->setFocus();
            return;
        }
        in.cost = *cost;
    }

    Status st = Status::ok();
    if (m_productId) {
        st = m_ui.app.catalog().updateProduct(m_ui.session, *m_productId, in);
    } else {
        auto created = m_ui.app.catalog().createProduct(m_ui.session, in);
        st = created.status();
    }
    if (!st) {
        m_error->setText(st.error());
        return;
    }
    emit m_ui.events->catalogChanged();
    emit m_ui.events->toast(m_productId ? QStringLiteral("Product saved") : QStringLiteral("Product created"));
    accept();
}

// ====================================================================== Categories
CategoriesDialog::CategoriesDialog(UiContext &ui, QWidget *parent)
    : QDialog(parent), m_ui(ui)
{
    setWindowTitle(QStringLiteral("Categories"));
    setMinimumSize(480, 440);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(12);
    v->addWidget(makeLabel(QStringLiteral("Categories"), "h2"));
    v->addWidget(makeLabel(QStringLiteral("Inactive categories, and every product in them, are hidden from customers."), "small"));

    auto *row = new QHBoxLayout();
    m_list = new QListWidget();
    row->addWidget(m_list, 1);

    auto *buttons = new QVBoxLayout();
    auto *add = makeButton(QStringLiteral("Add..."), "primary");
    auto *rename = makeButton(QStringLiteral("Rename..."));
    auto *toggle = makeButton(QStringLiteral("Activate / deactivate"));
    buttons->addWidget(add);
    buttons->addWidget(rename);
    buttons->addWidget(toggle);
    buttons->addStretch();
    auto *close = makeButton(QStringLiteral("Close"), "ghost");
    buttons->addWidget(close);
    row->addLayout(buttons);
    v->addLayout(row, 1);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("New category"), QStringLiteral("Category name:"),
                                                   QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        auto id = m_ui.app.catalog().createCategory(m_ui.session, name);
        if (!id) {
            showError(this, id.error());
            return;
        }
        emit m_ui.events->catalogChanged();
        reload();
    });
    connect(rename, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        if (id == 0)
            return;
        QString current;
        for (const Category &c : std::as_const(m_categories))
            if (c.id == id)
                current = c.name;
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("Rename category"), QStringLiteral("New name:"),
                                                   QLineEdit::Normal, current, &ok);
        if (!ok)
            return;
        const Status st = m_ui.app.catalog().renameCategory(m_ui.session, id, name);
        if (!st) {
            showError(this, st.error());
            return;
        }
        emit m_ui.events->catalogChanged();
        reload();
    });
    connect(toggle, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        for (const Category &c : std::as_const(m_categories)) {
            if (c.id != id)
                continue;
            const Status st = m_ui.app.catalog().setCategoryActive(m_ui.session, id, !c.active);
            if (!st) {
                showError(this, st.error());
                return;
            }
            emit m_ui.events->catalogChanged();
            reload();
            return;
        }
    });
    reload();
}

qint64 CategoriesDialog::selectedId() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toLongLong() : 0;
}

void CategoriesDialog::reload()
{
    const qint64 keep = selectedId();
    m_list->clear();
    auto cats = m_ui.app.catalog().categories(m_ui.session, true);
    if (!cats) {
        showError(this, cats.error());
        return;
    }
    m_categories = cats.value();
    for (const Category &c : std::as_const(m_categories)) {
        QString text = QStringLiteral("%1   ·   %2 product(s)").arg(c.name).arg(c.productCount);
        if (!c.active)
            text += QStringLiteral("   ·   inactive");
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, c.id);
        if (!c.active)
            item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        if (c.id == keep)
            m_list->setCurrentItem(item);
    }
}

// ====================================================================== Quantity prices
PriceTiersDialog::PriceTiersDialog(UiContext &ui, const Product &product, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_product(product)
{
    setWindowTitle(QStringLiteral("Quantity prices"));
    auto existing = ui.app.catalog().priceTiers(ui.session, product.id);
    if (!existing) {
        showError(parent, existing.error());
        return;
    }
    m_loaded = true;
    setMinimumWidth(580);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);
    root->addWidget(makeLabel(QStringLiteral("Quantity prices"), "h1"));

    QString facts = QStringLiteral("%1  ·  normal price %2").arg(product.name, product.price.toString());
    if (product.cost)
        facts += QStringLiteral("  ·  cost %1").arg(product.cost->toString());
    auto *about = makeLabel(facts, "muted");
    about->setWordWrap(true);
    root->addWidget(about);

    auto *hint = makeLabel(QStringLiteral("Give a lower price to customers who buy more of this product, for example "
                                          "12 or more at 5% off. The highest quantity a customer reaches applies, and "
                                          "a bigger quantity must get the same or a bigger discount. Leave the quantity "
                                          "at \"-\" to skip a row."),
                           "hint");
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);
    grid->addWidget(makeLabel(QStringLiteral("From quantity"), "small"), 0, 0);
    grid->addWidget(makeLabel(QStringLiteral("Discount (%)"), "small"), 0, 1);
    grid->addWidget(makeLabel(QStringLiteral("Price each"), "small"), 0, 2);
    for (int i = 0; i < kRows; ++i) {
        Row r;
        r.from = new QSpinBox();
        r.from->setRange(0, pricetier::kMaxQuantity);
        r.from->setSpecialValueText(QStringLiteral("-")); // 0 = row not used
        r.from->setMinimumHeight(34);
        r.from->setMinimumWidth(110);
        r.from->setAccessibleName(QStringLiteral("Tier %1: from quantity").arg(i + 1));
        r.discount = lineEdit(QStringLiteral("e.g. 5"));
        r.discount->setAccessibleName(QStringLiteral("Tier %1: discount percent").arg(i + 1));
        r.price = makeLabel(QString(), "h3");
        r.price->setMinimumWidth(170);
        grid->addWidget(r.from, i + 1, 0);
        grid->addWidget(r.discount, i + 1, 1);
        grid->addWidget(r.price, i + 1, 2);
        m_rows.push_back(r);
    }
    grid->setColumnStretch(2, 1);
    root->addLayout(grid);

    m_warnings = new QLabel();
    m_warnings->setWordWrap(true);
    m_warnings->hide();
    root->addWidget(m_warnings);

    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    root->addWidget(m_error);

    auto *buttons = new QHBoxLayout();
    auto *clearAll = makeButton(QStringLiteral("Remove all"), "ghost");
    clearAll->setToolTip(QStringLiteral("Empty every row. Save to sell at the normal price only."));
    connect(clearAll, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_rows.size(); ++i)
            setRow(i, 0, QString());
    });
    buttons->addWidget(clearAll);
    buttons->addStretch();
    auto *cancel = makeButton(QStringLiteral("Cancel"), "ghost");
    auto *saveButton = makeButton(QStringLiteral("Save prices"), "primary");
    saveButton->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, this, &PriceTiersDialog::save);
    buttons->addWidget(cancel);
    buttons->addWidget(saveButton);
    root->addLayout(buttons);

    const QVector<PriceTier> &saved = existing.value(); // sorted by quantity
    for (int i = 0; i < saved.size() && i < kRows; ++i)
        setRow(i, saved.at(i).minQuantity, pricetier::percentText(saved.at(i).discountBp));

    for (const Row &r : std::as_const(m_rows)) {
        connect(r.from, qOverload<int>(&QSpinBox::valueChanged), this, &PriceTiersDialog::updatePreview);
        connect(r.discount, &QLineEdit::textChanged, this, &PriceTiersDialog::updatePreview);
    }
    updatePreview();
}

void PriceTiersDialog::setRow(int row, int fromQuantity, const QString &discountPercent)
{
    if (row < 0 || row >= m_rows.size())
        return;
    m_rows[row].from->setValue(fromQuantity);
    m_rows[row].discount->setText(discountPercent);
}

Result<QVector<PriceTier>> PriceTiersDialog::collect() const
{
    QVector<PriceTier> tiers;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &r = m_rows.at(i);
        const QString text = r.discount->text().trimmed();
        if (r.from->value() == 0) {
            if (!text.isEmpty())
                return Result<QVector<PriceTier>>::fail(
                    QStringLiteral("Row %1: choose the quantity this price starts at, or clear the discount.").arg(i + 1));
            continue;
        }
        const std::optional<int> bp = pricetier::parsePercent(text);
        if (!bp)
            return Result<QVector<PriceTier>>::fail(
                QStringLiteral("Row %1: enter the discount as a percentage, e.g. 5 or 2.5.").arg(i + 1));
        PriceTier t;
        t.minQuantity = r.from->value();
        t.discountBp = *bp;
        tiers.push_back(t);
    }
    return tiers;
}

void PriceTiersDialog::updatePreview()
{
    // Price each for every complete row, and a warning for prices below cost.
    // The full rules are checked by the catalog service when saving.
    QVector<PriceTier> shown;
    for (const Row &r : std::as_const(m_rows)) {
        const std::optional<int> bp = pricetier::parsePercent(r.discount->text());
        if (r.from->value() >= pricetier::kMinQuantity && bp && *bp > 0 && *bp <= pricetier::kMaxDiscountBp) {
            PriceTier t;
            t.minQuantity = r.from->value();
            t.discountBp = *bp;
            r.price->setText(QStringLiteral("%1 each").arg(pricetier::unitPrice(m_product.price, t).toString()));
            shown.push_back(t);
        } else {
            r.price->clear();
        }
    }

    const QStringList warnings = CatalogService::belowCostWarnings(m_product.price, m_product.cost, shown);
    if (warnings.isEmpty()) {
        m_warnings->hide();
    } else {
        setBadge(m_warnings,
                 QStringLiteral("Below the cost price. This is allowed, but the shop loses money on:\n%1")
                     .arg(warnings.join(QLatin1Char('\n'))),
                 QStringLiteral("warning"));
        m_warnings->show();
    }
    m_error->clear();
}

void PriceTiersDialog::save()
{
    auto tiers = collect();
    if (!tiers) {
        m_error->setText(tiers.error());
        return;
    }
    const Status st = m_ui.app.catalog().setPriceTiers(m_ui.session, m_product.id, tiers.value());
    if (!st) {
        m_error->setText(st.error());
        return;
    }
    emit m_ui.events->catalogChanged();
    emit m_ui.events->toast(tiers.value().isEmpty()
                                ? QStringLiteral("%1 is back to its normal price only").arg(m_product.name)
                                : QStringLiteral("Quantity prices saved for %1").arg(m_product.name));
    accept();
}

// ====================================================================== Stock adjust
StockAdjustDialog::StockAdjustDialog(UiContext &ui, const Product &product, QWidget *parent)
    : QDialog(parent), m_ui(ui), m_product(product)
{
    setWindowTitle(QStringLiteral("Adjust stock"));
    setMinimumWidth(460);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    v->addWidget(makeLabel(QStringLiteral("Adjust stock"), "h2"));
    v->addWidget(makeLabel(QStringLiteral("%1  (%2)\nCurrent stock: %3").arg(product.name, product.sku).arg(product.stock), "muted"));

    m_reason = new QComboBox();
    m_reason->addItem(QStringLiteral("Restock - add received items"), int(StockReason::Restock));
    m_reason->addItem(QStringLiteral("Damaged / lost - remove items"), int(StockReason::Damaged));
    m_reason->addItem(QStringLiteral("Correction after a stock count (+/-)"), int(StockReason::Adjustment));
    m_reason->setMinimumHeight(34);
    m_quantity = new QSpinBox();
    m_quantity->setMinimumHeight(34);
    m_note = lineEdit(QStringLiteral("Note (required for corrections)"));

    auto *form = new QFormLayout();
    form->setVerticalSpacing(10);
    form->addRow(makeFormLabel(QStringLiteral("Reason")), m_reason);
    form->addRow(makeFormLabel(QStringLiteral("Quantity")), m_quantity);
    form->addRow(makeFormLabel(QStringLiteral("Note")), m_note);
    v->addLayout(form);

    m_preview = makeLabel(QString(), "h3");
    v->addWidget(m_preview);
    m_error = makeLabel(QString(), "error");
    m_error->setWordWrap(true);
    v->addWidget(m_error);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = makeButton(QStringLiteral("Cancel"), "ghost");
    auto *save = makeButton(QStringLiteral("Save adjustment"), "primary");
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    v->addLayout(buttons);

    auto configure = [this] {
        if (reason() == StockReason::Adjustment) {
            m_quantity->setRange(-InventoryService::kMaxAdjustment, InventoryService::kMaxAdjustment);
            m_quantity->setPrefix(QString());
        } else {
            m_quantity->setRange(1, reason() == StockReason::Damaged ? qMax(1, m_product.stock) : InventoryService::kMaxAdjustment);
            m_quantity->setPrefix(reason() == StockReason::Damaged ? QStringLiteral("- ") : QStringLiteral("+ "));
        }
        if (m_quantity->value() == 0)
            m_quantity->setValue(1);
        updatePreview();
    };
    connect(m_reason, qOverload<int>(&QComboBox::currentIndexChanged), this, configure);
    connect(m_quantity, qOverload<int>(&QSpinBox::valueChanged), this, &StockAdjustDialog::updatePreview);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &StockAdjustDialog::save);
    m_quantity->setValue(1);
    configure();
}

StockReason StockAdjustDialog::reason() const
{
    return static_cast<StockReason>(m_reason->currentData().toInt());
}

int StockAdjustDialog::delta() const
{
    return reason() == StockReason::Damaged ? -m_quantity->value() : m_quantity->value();
}

void StockAdjustDialog::updatePreview()
{
    const int after = m_product.stock + delta();
    m_preview->setText(QStringLiteral("New stock will be: %1").arg(after));
}

void StockAdjustDialog::save()
{
    auto result = m_ui.app.inventory().adjust(m_ui.session, m_product.id, delta(), reason(), m_note->text());
    if (!result) {
        m_error->setText(result.error());
        return;
    }
    emit m_ui.events->catalogChanged();
    emit m_ui.events->toast(QStringLiteral("%1 stock is now %2").arg(m_product.name).arg(result.value()));
    accept();
}

} // namespace shophub::ui
