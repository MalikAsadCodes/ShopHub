#pragma once

#include "ui/UiCommon.h"

#include <QVBoxLayout>
#include <QWidget>

class QScrollArea;

namespace shophub::ui {

// Base for every screen in the main window. refresh() reloads data from
// the services; the main window calls it whenever the page is shown.
class Page : public QWidget
{
    Q_OBJECT
public:
    Page(UiContext &ui, QWidget *parent = nullptr);
    virtual void refresh() = 0;
    // Optional hook for the top-bar search box.
    virtual void applySearch(const QString &) {}

protected:
    // Adds "Title" + optional subtitle at the top of the page.
    void addHeader(const QString &title, const QString &subtitle = QString(), QWidget *trailing = nullptr);
    // Wraps a widget in a borderless scroll area.
    static QScrollArea *scrollWrap(QWidget *content);

    UiContext &m_ui;
    QVBoxLayout *m_layout;
};

} // namespace shophub::ui
