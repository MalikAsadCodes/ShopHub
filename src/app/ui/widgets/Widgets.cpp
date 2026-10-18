#include "ui/widgets/Widgets.h"

#include "ui/Theme.h"
#include "ui/UiCommon.h"

#include <QGraphicsOpacityEffect>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

namespace shophub::ui {

// ============================================================== StatCard
StatCard::StatCard(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Card"));
    setMinimumWidth(170);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(4);
    layout->addWidget(makeLabel(title, "muted"));
    m_value = makeLabel(QStringLiteral("-"), "statvalue");
    layout->addWidget(m_value);
    m_hint = makeLabel(QString(), "small");
    layout->addWidget(m_hint);
}

void StatCard::setValue(const QString &value, const QString &hint, const QString &hintRole)
{
    m_value->setText(value);
    m_hint->setText(hint);
    m_hint->setProperty("role", hintRole);
    repolish(m_hint);
}

// ============================================================== SalesChart
SalesChart::SalesChart(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setMouseTracking(true);
}

void SalesChart::setData(const QVector<DailySales> &days)
{
    m_days = days;
    m_max = 0;
    for (const DailySales &d : days)
        m_max = qMax(m_max, d.revenue.paisa());
    m_hover = -1;
    update();
}

QRectF SalesChart::plotArea() const
{
    return QRectF(56, 12, width() - 56 - 8, height() - 12 - 28);
}

QRectF SalesChart::barRect(int index) const
{
    const QRectF area = plotArea();
    const qreal slot = area.width() / qMax(1, m_days.size());
    const qreal barW = qMin<qreal>(28.0, slot * 0.62);
    const qreal ratio = m_max > 0 ? qreal(m_days[index].revenue.paisa()) / qreal(m_max) : 0.0;
    const qreal h = qMax<qreal>(m_days[index].revenue.paisa() > 0 ? 3.0 : 0.0, area.height() * ratio);
    return QRectF(area.left() + slot * index + (slot - barW) / 2, area.bottom() - h, barW, h);
}

void SalesChart::paintEvent(QPaintEvent *)
{
    const ThemePalette &pal = Theme::palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area = plotArea();

    // Grid lines with rupee labels (4 steps)
    QFont small = font();
    small.setPixelSize(11);
    p.setFont(small);
    for (int i = 0; i <= 4; ++i) {
        const qreal y = area.bottom() - area.height() * i / 4.0;
        p.setPen(QPen(pal.border, 1, i == 0 ? Qt::SolidLine : Qt::DashLine));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        const qint64 rupees = m_max * i / 4 / 100;
        QString label = rupees >= 1000 ? QStringLiteral("%1k").arg(rupees / 1000.0, 0, 'f', rupees >= 10000 ? 0 : 1)
                                       : QString::number(rupees);
        p.setPen(pal.muted);
        p.drawText(QRectF(0, y - 8, area.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    if (m_days.isEmpty())
        return;

    const QLocale en(QLocale::English);
    for (int i = 0; i < m_days.size(); ++i) {
        const QRectF bar = barRect(i);
        QColor c = i == m_hover ? pal.accentHover : pal.accent;
        if (m_days[i].revenue.isZero())
            c = pal.border;
        QPainterPath path;
        path.addRoundedRect(bar, 4, 4);
        p.fillPath(path, c);

        // Day labels: every other day to avoid crowding, plus today.
        if (i % 2 == 0 || i == m_days.size() - 1) {
            const qreal slot = area.width() / m_days.size();
            p.setPen(i == m_days.size() - 1 ? pal.text : pal.muted);
            const QString text = i == m_days.size() - 1 ? QStringLiteral("Today") : en.toString(m_days[i].date, QStringLiteral("d MMM"));
            p.drawText(QRectF(area.left() + slot * i - 10, area.bottom() + 6, slot + 20, 18), Qt::AlignHCenter | Qt::AlignTop, text);
        }
    }

    if (m_max == 0) {
        p.setPen(pal.muted);
        p.drawText(area, Qt::AlignCenter, QStringLiteral("No sales in the last 14 days yet"));
    }
}

void SalesChart::mouseMoveEvent(QMouseEvent *event)
{
    int hover = -1;
    const QRectF area = plotArea();
    if (!m_days.isEmpty() && area.contains(event->position())) {
        const qreal slot = area.width() / m_days.size();
        hover = qBound(0, int((event->position().x() - area.left()) / slot), int(m_days.size()) - 1);
    }
    if (hover != m_hover) {
        m_hover = hover;
        update();
        if (hover >= 0) {
            const DailySales &d = m_days[hover];
            QToolTip::showText(event->globalPosition().toPoint(),
                               QStringLiteral("%1\n%2 - %3 order(s)")
                                   .arg(QLocale(QLocale::English).toString(d.date, QStringLiteral("ddd d MMM")),
                                        d.revenue.toString())
                                   .arg(d.orders),
                               this);
        } else {
            QToolTip::hideText();
        }
    }
}

void SalesChart::leaveEvent(QEvent *event)
{
    m_hover = -1;
    update();
    QWidget::leaveEvent(event);
}

// ============================================================== EmptyState
EmptyState::EmptyState(const QString &title, const QString &message, const QString &actionText, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 48, 24, 48);
    layout->setSpacing(8);
    layout->addStretch();
    m_title = makeLabel(title, "h2");
    m_title->setAlignment(Qt::AlignCenter);
    m_message = makeLabel(message, "muted");
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(m_message);
    if (!actionText.isEmpty()) {
        auto *button = makeButton(actionText, "primary");
        connect(button, &QPushButton::clicked, this, &EmptyState::actionClicked);
        layout->addSpacing(8);
        layout->addWidget(button, 0, Qt::AlignHCenter);
    }
    layout->addStretch();
}

void EmptyState::setTexts(const QString &title, const QString &message)
{
    m_title->setText(title);
    m_message->setText(message);
}

// ============================================================== Toast
void Toast::show(QWidget *window, const QString &message)
{
    if (!window)
        return;
    auto *label = new QLabel(message, window);
    label->setObjectName(QStringLiteral("Toast"));
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->adjustSize();
    label->move((window->width() - label->width()) / 2, window->height() - label->height() - 32);
    label->show();
    label->raise();

    auto *effect = new QGraphicsOpacityEffect(label);
    effect->setOpacity(1.0);
    label->setGraphicsEffect(effect);

    QTimer::singleShot(2200, label, [label, effect] {
        auto *fade = new QPropertyAnimation(effect, "opacity", label);
        fade->setDuration(350);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        QObject::connect(fade, &QPropertyAnimation::finished, label, &QLabel::deleteLater);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

} // namespace shophub::ui
