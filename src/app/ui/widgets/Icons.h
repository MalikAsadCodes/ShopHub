#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace shophub::ui {

// Line icons drawn with QPainter (no icon font or image files), on a 24-unit grid
// with round ends, so they stay sharp at any size and take any theme colour.
enum class Icon {
    Search,
    Cart,
    User,
    Orders,   // a parcel
    Truck,
    Cash,
    Return,   // an arrow turning back
    Wallet,
    ChevronDown,
    ChevronRight,
    Moon,
    Sun,
    Key,
    SignOut,
    Grid,
    Close,
};

// The icon in one colour, at 1x and 2x so it is sharp on high-DPI screens.
QIcon makeIcon(Icon icon, const QColor &color, int size = 20);
QPixmap iconPixmap(Icon icon, const QColor &color, int size, qreal devicePixelRatio = 2.0);

} // namespace shophub::ui
