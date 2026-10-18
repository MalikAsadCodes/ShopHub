#pragma once

#include "domain/Session.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace shophub {
class AppContext;
}

namespace shophub::ui {

// Sign in / create account. In FirstRun mode it instead creates the very
// first administrator account (and can load the optional demo data).
class AuthDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { SignIn, FirstRun };

    AuthDialog(AppContext &app, Mode mode, QWidget *parent = nullptr);
    const Session &session() const { return m_session; }

private:
    QWidget *buildBrandPanel();
    QWidget *buildSignInPage();
    QWidget *buildRegisterPage();
    QWidget *buildFirstRunPage();
    void showPage(int index);
    void doSignIn();
    void doRegister();
    void doFirstRun();
    void setBusy(QPushButton *button, bool busy, const QString &idleText);

    AppContext &m_app;
    Session m_session;
    QStackedWidget *m_stack = nullptr;

    QLineEdit *m_loginEmail = nullptr;
    QLineEdit *m_loginPassword = nullptr;
    QLabel *m_loginError = nullptr;
    QPushButton *m_loginButton = nullptr;

    QLineEdit *m_regName = nullptr;
    QLineEdit *m_regEmail = nullptr;
    QLineEdit *m_regPassword = nullptr;
    QLineEdit *m_regConfirm = nullptr;
    QLabel *m_regError = nullptr;
    QPushButton *m_regButton = nullptr;

    QLineEdit *m_adminName = nullptr;
    QLineEdit *m_adminEmail = nullptr;
    QLineEdit *m_adminPassword = nullptr;
    QLineEdit *m_adminConfirm = nullptr;
    QCheckBox *m_loadDemo = nullptr;
    QLabel *m_adminError = nullptr;
    QPushButton *m_adminButton = nullptr;
};

} // namespace shophub::ui
