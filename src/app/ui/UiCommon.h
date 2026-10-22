#pragma once

#include "domain/Session.h"

#include <QFrame>
#include <QLabel>
#include <QObject>
#include <QPushButton>

class QLayout;
class QTableView;

namespace shophub {
class AppContext;
}

namespace shophub::ui {

// App-wide notifications so pages never need to know about each other.
class AppEvents : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

signals:
    void cartChanged();
    void ordersChanged();
    void catalogChanged();
    void khataChanged();
    void toast(const QString &message);
    void openShop(qint64 categoryId, const QString &searchText);
    void openProduct(qint64 productId);
    void navigate(const QString &pageKey); // "home", "shop", "cart", "orders", ...
};

// What every page and dialog needs: the back end, who is signed in,
// and the event hub.
struct UiContext
{
    AppContext &app;
    Session session;
    AppEvents *events = nullptr;
};

// ---- small widget factories (keep styling in the style sheet, not in code)
QLabel *makeLabel(const QString &text, const char *role = nullptr, QWidget *parent = nullptr);
QPushButton *makeButton(const QString &text, const char *variant = nullptr, QWidget *parent = nullptr);
QLabel *makeBadge(const QString &text, const QString &kind, QWidget *parent = nullptr);
void setBadge(QLabel *label, const QString &text, const QString &kind);
QFrame *makeCard(QWidget *parent = nullptr);
QFrame *makeDivider(QWidget *parent = nullptr);
QLabel *makeFormLabel(const QString &text);          // muted label sized to sit level with an input
QString escapeMnemonic(QString text);                // "Home & Kitchen" -> "Home && Kitchen" for buttons
void repolish(QWidget *w);
void clearLayout(QLayout *layout);          // deletes every widget and sub-layout in it
void setupTable(QTableView *table);         // shared look & behaviour for data tables

// ---- dialogs
void showError(QWidget *parent, const QString &message);
void showInfo(QWidget *parent, const QString &title, const QString &message);
bool confirm(QWidget *parent, const QString &title, const QString &message, const QString &okText);

// ---- formatting
QString formatDateTime(const QDateTime &utc);
QString formatDate(const QDateTime &utc);
QString initials(const QString &name);
QString stockText(int stock, int lowThreshold);
QString stockBadgeKind(int stock, int lowThreshold);

} // namespace shophub::ui
