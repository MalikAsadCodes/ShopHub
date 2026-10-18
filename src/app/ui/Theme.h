#pragma once

#include "domain/OrderStatus.h"

#include <QColor>
#include <QString>

namespace shophub::ui {

enum class ThemeMode { Light, Dark };

// Design tokens. Every colour in the app comes from here, so light and dark
// themes stay consistent and custom-painted widgets match the style sheet.
struct ThemePalette
{
    QColor bg, surface, surfaceAlt, border, borderStrong;
    QColor text, muted;
    QColor accent, accentHover, accentSoft, accentText, heroEnd;
    QColor danger, dangerSoft, warning, warningSoft, success, successSoft, info, infoSoft;
    QColor sidebar, sidebarHover, sidebarActive, sidebarText, sidebarMuted;
};

class Theme
{
public:
    static void apply(ThemeMode mode);
    static ThemeMode current();
    static const ThemePalette &palette();

    static ThemeMode loadSaved();
    static void save(ThemeMode mode);

    // "success" / "warning" / "danger" / "info" / "neutral" badge kind for a status.
    static QString badgeKind(OrderStatus status);
    static QColor statusColor(OrderStatus status);
};

} // namespace shophub::ui
