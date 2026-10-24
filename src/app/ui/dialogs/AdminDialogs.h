#pragma once

#include "common/Result.h"
#include "domain/Types.h"
#include "ui/UiCommon.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;

namespace shophub::ui {

class ImageView;

// Create a new product, or edit an existing one (pass its id).
class ProductEditDialog : public QDialog
{
    Q_OBJECT
public:
    ProductEditDialog(UiContext &ui, std::optional<qint64> productId, QWidget *parent = nullptr);
    bool isLoaded() const { return m_loaded; }

private:
    void loadCategories(qint64 selectedId);
    void chooseImage();
    void updatePreview();
    void save();

    UiContext &m_ui;
    std::optional<qint64> m_productId;
    bool m_loaded = true;
    QString m_imageFile;

    QLineEdit *m_sku = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_category = nullptr;
    QLineEdit *m_brand = nullptr;
    QLineEdit *m_price = nullptr;
    QLineEdit *m_cost = nullptr;
    QSpinBox *m_stock = nullptr;
    QSpinBox *m_lowStock = nullptr;
    QPlainTextEdit *m_description = nullptr;
    ImageView *m_preview = nullptr;
    QLabel *m_error = nullptr;
};

class CategoriesDialog : public QDialog
{
    Q_OBJECT
public:
    CategoriesDialog(UiContext &ui, QWidget *parent = nullptr);

private:
    void reload();
    qint64 selectedId() const;

    UiContext &m_ui;
    QListWidget *m_list = nullptr;
    QVector<Category> m_categories;
};

// Wholesale (quantity) prices of one product: up to 5 rows of
// "from N units, X% off", with the price each and a cost-price warning.
class PriceTiersDialog : public QDialog
{
    Q_OBJECT
public:
    static constexpr int kRows = pricetier::kMaxTiersPerProduct;

    PriceTiersDialog(UiContext &ui, const Product &product, QWidget *parent = nullptr);
    bool isLoaded() const { return m_loaded; }

    // For tests: fill row `row` (0-based); quantity 0 leaves the row empty.
    void setRow(int row, int fromQuantity, const QString &discountPercent);
    void save();

private:
    struct Row
    {
        QSpinBox *from = nullptr;
        QLineEdit *discount = nullptr;
        QLabel *price = nullptr;
    };

    // The rows as tiers, or an error message for the admin.
    Result<QVector<PriceTier>> collect() const;
    void updatePreview();

    UiContext &m_ui;
    Product m_product;
    bool m_loaded = false;
    QVector<Row> m_rows;
    QLabel *m_warnings = nullptr;
    QLabel *m_error = nullptr;
};

class StockAdjustDialog : public QDialog
{
    Q_OBJECT
public:
    StockAdjustDialog(UiContext &ui, const Product &product, QWidget *parent = nullptr);

private:
    int delta() const;
    StockReason reason() const;
    void updatePreview();
    void save();

    UiContext &m_ui;
    Product m_product;
    QComboBox *m_reason = nullptr;
    QSpinBox *m_quantity = nullptr;
    QLineEdit *m_note = nullptr;
    QLabel *m_preview = nullptr;
    QLabel *m_error = nullptr;
};

} // namespace shophub::ui
