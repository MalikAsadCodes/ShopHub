#pragma once

#include "ui/UiCommon.h"

#include <QMainWindow>
#include <QMap>

#include <memory>

class QButtonGroup;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace shophub {
class AppContext;
}

namespace shophub::ui {

class Page;
class ShopPage;

// Sidebar + top bar + stacked pages. Which pages exist depends on the role.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(AppContext &app, const Session &session, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Used by tests and by the event hub.
    void navigate(const QString &key, bool refresh = true);
    QString currentPageKey() const;
    Page *page(const QString &key) const { return m_pages.value(key); }
    UiContext &uiContext() { return m_ui; }

signals:
    void signedOut();

private:
    QWidget *buildSidebar();
    QWidget *buildTopBar();
    void addPage(const QString &key, const QString &label, Page *page);
    void refreshCurrent();
    void updateCartBadge();
    void openProduct(qint64 productId);
    void toggleTheme();
    void signOut();
    void runSearch();

    std::unique_ptr<AppEvents> m_events;
    UiContext m_ui;

    QStackedWidget *m_stack = nullptr;
    QVBoxLayout *m_navLayout = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QMap<QString, Page *> m_pages;
    QMap<QString, QPushButton *> m_navButtons;
    QStringList m_order;
    ShopPage *m_shop = nullptr;

    QLineEdit *m_search = nullptr;
    QPushButton *m_cartButton = nullptr;
    QPushButton *m_themeButton = nullptr;
};

} // namespace shophub::ui
