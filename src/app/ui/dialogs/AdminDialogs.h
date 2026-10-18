#pragma once

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
