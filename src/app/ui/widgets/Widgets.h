#pragma once

#include "domain/Types.h"

#include <QFrame>
#include <QLabel>

class QPushButton;

namespace shophub::ui {

// Dashboard tile: small title, big number, optional hint underneath.
class StatCard : public QFrame
{
    Q_OBJECT
public:
    explicit StatCard(const QString &title, QWidget *parent = nullptr);
    void setValue(const QString &value, const QString &hint = QString(), const QString &hintRole = QStringLiteral("small"));

private:
    QLabel *m_value;
    QLabel *m_hint;
};

// Painted bar chart of daily sales (no Qt Charts dependency).
class SalesChart : public QWidget
{
    Q_OBJECT
public:
    explicit SalesChart(QWidget *parent = nullptr);
    void setData(const QVector<DailySales> &days);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF barRect(int index) const;
    QRectF plotArea() const;

    QVector<DailySales> m_days;
    qint64 m_max = 0;
    int m_hover = -1;
};

// Friendly message shown when a list has nothing in it.
class EmptyState : public QWidget
{
    Q_OBJECT
public:
    EmptyState(const QString &title, const QString &message, const QString &actionText = QString(),
               QWidget *parent = nullptr);
    void setTexts(const QString &title, const QString &message);

signals:
    void actionClicked();

private:
    QLabel *m_title;
    QLabel *m_message;
};

// A short message that fades away at the bottom of a window.
class Toast
{
public:
    static void show(QWidget *window, const QString &message);
};

} // namespace shophub::ui
