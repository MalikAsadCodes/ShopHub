#pragma once

#include "domain/Types.h"
#include "ui/UiCommon.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QRadioButton;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;

namespace shophub::ui {

class ProductDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    ProductDetailsDialog(UiContext &ui, qint64 productId, QWidget *parent = nullptr);
    bool isLoaded() const { return m_loaded; }

private:
    UiContext &m_ui;
    Product m_product;
    bool m_loaded = false;
};

class CheckoutDialog : public QDialog
{
    Q_OBJECT
public:
    CheckoutDialog(UiContext &ui, QWidget *parent = nullptr);
    const Order &placedOrder() const { return m_order; }

    // For tests: pick a qist plan by its position in the list.
    void selectQistPlan(int index);

private:
    void placeOrder();
    void updateQist();          // shows or hides the qist choices and their preview
    std::optional<Money> qistDown(bool &valid) const; // empty = the plan's minimum

    UiContext &m_ui;
    QString m_token; // one per dialog: protects against double submission
    Order m_order;
    Money m_total;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_phone = nullptr;
    QLineEdit *m_address = nullptr;
    QLineEdit *m_city = nullptr;
    QCheckBox *m_saveAddress = nullptr;
    QRadioButton *m_khata = nullptr;
    QRadioButton *m_qist = nullptr;
    QLabel *m_qistHint = nullptr;
    QWidget *m_qistBox = nullptr;
    QComboBox *m_qistPlan = nullptr;
    QLineEdit *m_qistDown = nullptr;
    QLabel *m_qistPreview = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_placeButton = nullptr;
};

class OrderDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    OrderDetailsDialog(UiContext &ui, qint64 orderId, QWidget *parent = nullptr);
    bool isLoaded() const { return m_loaded; }

private:
    void reload();
    void advance();
    void cancelOrder();

    UiContext &m_ui;
    qint64 m_orderId;
    Order m_order;
    bool m_loaded = false;
    QVBoxLayout *m_body = nullptr;
    QWidget *m_content = nullptr;
};

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT
public:
    ChangePasswordDialog(UiContext &ui, QWidget *parent = nullptr);

private:
    UiContext &m_ui;
};

} // namespace shophub::ui
