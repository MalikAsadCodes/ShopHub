#include "ui/widgets/Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace shophub::ui {

namespace {

// Strokes are drawn on a 24 x 24 grid; `fills` are solid shapes (wheels, dots).
struct Drawing
{
    QPainterPath strokes;
    QPainterPath fills;
};

Drawing drawingFor(Icon icon)
{
    Drawing d;
    QPainterPath &p = d.strokes;
    switch (icon) {
    case Icon::Search:
        p.addEllipse(QPointF(10.5, 10.5), 6.5, 6.5);
        p.moveTo(15.5, 15.5);
        p.lineTo(20.5, 20.5);
        break;
    case Icon::Cart:
        p.moveTo(2.5, 4);
        p.lineTo(5.2, 4);
        p.lineTo(7.7, 15);
        p.lineTo(18, 15);
        p.lineTo(20.6, 7.2);
        p.lineTo(6, 7.2);
        d.fills.addEllipse(QPointF(9, 19.3), 1.5, 1.5);
        d.fills.addEllipse(QPointF(16.8, 19.3), 1.5, 1.5);
        break;
    case Icon::User:
        p.addEllipse(QPointF(12, 8.3), 3.9, 3.9);
        p.moveTo(4.6, 20);
        p.cubicTo(5.6, 15.6, 8.6, 13.9, 12, 13.9);
        p.cubicTo(15.4, 13.9, 18.4, 15.6, 19.4, 20);
        break;
    case Icon::Orders:
        p.moveTo(4, 7.8);
        p.lineTo(12, 3.8);
        p.lineTo(20, 7.8);
        p.lineTo(20, 16.4);
        p.lineTo(12, 20.4);
        p.lineTo(4, 16.4);
        p.closeSubpath();
        p.moveTo(4, 7.8);
        p.lineTo(12, 11.8);
        p.lineTo(20, 7.8);
        p.moveTo(12, 11.8);
        p.lineTo(12, 20.4);
        p.moveTo(8, 5.8);
        p.lineTo(16, 9.8);
        break;
    case Icon::Truck:
        p.addRoundedRect(QRectF(2.5, 6, 11.5, 10.5), 1.2, 1.2);
        p.moveTo(14, 9.5);
        p.lineTo(17.8, 9.5);
        p.lineTo(21, 12.8);
        p.lineTo(21, 16.5);
        p.lineTo(14, 16.5);
        p.addEllipse(QPointF(7, 17.6), 1.9, 1.9);
        p.addEllipse(QPointF(17, 17.6), 1.9, 1.9);
        break;
    case Icon::Cash:
        p.addRoundedRect(QRectF(2.5, 6.5, 19, 11), 2, 2);
        p.addEllipse(QPointF(12, 12), 2.7, 2.7);
        d.fills.addEllipse(QPointF(6.2, 12), 1, 1);
        d.fills.addEllipse(QPointF(17.8, 12), 1, 1);
        break;
    case Icon::Return:
        p.moveTo(9, 5);
        p.lineTo(4.5, 9.5);
        p.lineTo(9, 14);
        p.moveTo(4.5, 9.5);
        p.lineTo(14.5, 9.5);
        p.cubicTo(17.6, 9.5, 19.6, 11.6, 19.6, 14.5);
        p.cubicTo(19.6, 17.5, 17.5, 19.5, 14.5, 19.5);
        p.lineTo(10, 19.5);
        break;
    case Icon::Wallet:
        p.addRoundedRect(QRectF(3, 6.5, 18, 13), 2.5, 2.5);
        p.addRoundedRect(QRectF(15, 11, 6, 4.2), 1.4, 1.4);
        p.moveTo(5.2, 6.5);
        p.lineTo(15.6, 3.6);
        p.lineTo(16.6, 6.5);
        break;
    case Icon::ChevronDown:
        p.moveTo(6, 9);
        p.lineTo(12, 15);
        p.lineTo(18, 9);
        break;
    case Icon::ChevronRight:
        p.moveTo(9, 6);
        p.lineTo(15, 12);
        p.lineTo(9, 18);
        break;
    case Icon::Moon:
    {
        // A full moon with a smaller circle taken out of its top right.
        QPainterPath full;
        full.addEllipse(QPointF(11.2, 12.8), 7.8, 7.8);
        QPainterPath bite;
        bite.addEllipse(QPointF(16.6, 8.2), 6.4, 6.4);
        p = full.subtracted(bite);
        break;
    }
    case Icon::Sun:
        p.addEllipse(QPointF(12, 12), 4, 4);
        for (int i = 0; i < 8; ++i) {
            const qreal a = qDegreesToRadians(45.0 * i);
            p.moveTo(12 + 7 * qCos(a), 12 + 7 * qSin(a));
            p.lineTo(12 + 9.4 * qCos(a), 12 + 9.4 * qSin(a));
        }
        break;
    case Icon::Key:
        p.addEllipse(QPointF(8, 16), 3.7, 3.7);
        p.moveTo(10.7, 13.3);
        p.lineTo(19.5, 4.5);
        p.moveTo(16.2, 7.8);
        p.lineTo(18.8, 10.4);
        p.moveTo(13.9, 10.1);
        p.lineTo(15.8, 12);
        break;
    case Icon::SignOut:
        p.moveTo(10, 4);
        p.lineTo(6.2, 4);
        p.quadTo(4, 4, 4, 6.2);
        p.lineTo(4, 17.8);
        p.quadTo(4, 20, 6.2, 20);
        p.lineTo(10, 20);
        p.moveTo(14.5, 8);
        p.lineTo(18.5, 12);
        p.lineTo(14.5, 16);
        p.moveTo(9.5, 12);
        p.lineTo(18.5, 12);
        break;
    case Icon::Grid:
        p.addRoundedRect(QRectF(3.5, 3.5, 7, 7), 1.6, 1.6);
        p.addRoundedRect(QRectF(13.5, 3.5, 7, 7), 1.6, 1.6);
        p.addRoundedRect(QRectF(3.5, 13.5, 7, 7), 1.6, 1.6);
        p.addRoundedRect(QRectF(13.5, 13.5, 7, 7), 1.6, 1.6);
        break;
    case Icon::Close:
        p.moveTo(6.5, 6.5);
        p.lineTo(17.5, 17.5);
        p.moveTo(17.5, 6.5);
        p.lineTo(6.5, 17.5);
        break;
    }
    return d;
}

} // namespace

QPixmap iconPixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio)
{
    QPixmap pm(QSize(size, size) * devicePixelRatio);
    pm.setDevicePixelRatio(devicePixelRatio);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);
    const Drawing d = drawingFor(icon);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(d.strokes);
    if (!d.fills.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(d.fills);
    }
    return pm;
}

QIcon makeIcon(Icon icon, const QColor &color, int size)
{
    QIcon result;
    result.addPixmap(iconPixmap(icon, color, size, 1.0));
    result.addPixmap(iconPixmap(icon, color, size, 2.0));
    return result;
}

} // namespace shophub::ui
