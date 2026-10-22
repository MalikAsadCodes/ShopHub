#pragma once

#include "domain/Types.h"
#include "ui/UiCommon.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;

namespace shophub::ui {

// Admin: one small form for the three things a shopkeeper does in a khata.
class KhataDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { SetLimit, RecordPayment, Adjust };

    KhataDialog(UiContext &ui, const KhataAccount &account, Mode mode, QWidget *parent = nullptr);

private:
    std::optional<Money> amount() const;
    void updatePreview();
    void save();

    UiContext &m_ui;
    KhataAccount m_account;
    Mode m_mode;
    QLineEdit *m_amount = nullptr;
    QComboBox *m_choice = nullptr;   // payment method, or adjustment direction
    QLineEdit *m_note = nullptr;
    QLabel *m_preview = nullptr;
    QLabel *m_error = nullptr;
};

} // namespace shophub::ui
