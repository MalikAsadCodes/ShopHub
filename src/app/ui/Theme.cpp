#include "ui/Theme.h"

#include <QApplication>
#include <QMap>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTemporaryDir>

#include <memory>

namespace shophub::ui {

namespace {

ThemeMode g_mode = ThemeMode::Light;

ThemePalette lightPalette()
{
    ThemePalette p;
    p.bg = QColor(0xF5, 0xF6, 0xF8);
    p.surface = QColor(0xFF, 0xFF, 0xFF);
    p.surfaceAlt = QColor(0xF1, 0xF3, 0xF6);
    p.border = QColor(0xE2, 0xE5, 0xEA);
    p.borderStrong = QColor(0xB9, 0xC1, 0xCC);
    p.text = QColor(0x15, 0x19, 0x20);
    p.muted = QColor(0x5D, 0x66, 0x74);
    p.accent = QColor(0x0E, 0x7C, 0x66);
    p.accentHover = QColor(0x0A, 0x65, 0x53);
    p.accentSoft = QColor(0xE1, 0xF3, 0xEE);
    p.accentText = QColor(0xFF, 0xFF, 0xFF);
    p.heroEnd = QColor(0x13, 0x4E, 0x6F);
    p.danger = QColor(0xC0, 0x36, 0x2C);
    p.dangerSoft = QColor(0xFC, 0xEA, 0xE8);
    p.warning = QColor(0xA8, 0x62, 0x07);
    p.warningSoft = QColor(0xFD, 0xF2, 0xDE);
    p.success = QColor(0x1F, 0x7A, 0x45);
    p.successSoft = QColor(0xE3, 0xF5, 0xEA);
    p.info = QColor(0x1F, 0x5F, 0xB8);
    p.infoSoft = QColor(0xE6, 0xEF, 0xFB);
    p.sidebar = QColor(0x10, 0x18, 0x22);
    p.sidebarHover = QColor(0x1B, 0x26, 0x33);
    p.sidebarActive = QColor(0x0E, 0x7C, 0x66);
    p.sidebarText = QColor(0xC3, 0xCC, 0xD8);
    p.sidebarMuted = QColor(0x7D, 0x89, 0x99);
    return p;
}

ThemePalette darkPalette()
{
    ThemePalette p;
    p.bg = QColor(0x0D, 0x11, 0x16);
    p.surface = QColor(0x16, 0x1B, 0x22);
    p.surfaceAlt = QColor(0x1C, 0x22, 0x2B);
    p.border = QColor(0x2B, 0x33, 0x3E);
    p.borderStrong = QColor(0x47, 0x52, 0x60);
    p.text = QColor(0xE7, 0xEB, 0xF0);
    p.muted = QColor(0x98, 0xA2, 0xB0);
    p.accent = QColor(0x2F, 0xB8, 0x98);
    p.accentHover = QColor(0x4C, 0xCC, 0xAE);
    p.accentSoft = QColor(0x14, 0x37, 0x31);
    p.accentText = QColor(0x06, 0x12, 0x0F);
    p.heroEnd = QColor(0x1D, 0x4F, 0x7A);
    p.danger = QColor(0xF2, 0x6D, 0x62);
    p.dangerSoft = QColor(0x3A, 0x1C, 0x1A);
    p.warning = QColor(0xE8, 0xA9, 0x3C);
    p.warningSoft = QColor(0x36, 0x2A, 0x14);
    p.success = QColor(0x4C, 0xC7, 0x7F);
    p.successSoft = QColor(0x15, 0x33, 0x23);
    p.info = QColor(0x6A, 0xA6, 0xF5);
    p.infoSoft = QColor(0x17, 0x28, 0x40);
    p.sidebar = QColor(0x09, 0x0C, 0x10);
    p.sidebarHover = QColor(0x15, 0x1B, 0x23);
    p.sidebarActive = QColor(0x14, 0x37, 0x31);
    p.sidebarText = QColor(0xB7, 0xC0, 0xCC);
    p.sidebarMuted = QColor(0x6E, 0x79, 0x88);
    return p;
}

ThemePalette g_palette = lightPalette();

// {token} placeholders are replaced with palette colours.
const char *kStyleSheet = R"QSS(
QMainWindow, QDialog { background: {bg}; }
QWidget { color: {text}; }
QLabel { background: transparent; }
QToolTip { background: {text}; color: {surface}; border: none; padding: 5px 8px; border-radius: 6px; }

QScrollArea, QScrollArea > QWidget > QWidget#ScrollContent { background: transparent; border: none; }
QScrollArea > QWidget#qt_scrollarea_viewport { background: transparent; }

/* ---------- typography ---------- */
QLabel[role="h1"] { font-size: 24px; font-weight: 700; }
QLabel[role="h2"] { font-size: 17px; font-weight: 700; }
QLabel[role="h3"] { font-size: 14px; font-weight: 600; }
QLabel[role="muted"] { color: {muted}; }
QLabel[role="small"] { color: {muted}; font-size: 11px; }
QLabel[role="price"] { color: {accent}; font-size: 16px; font-weight: 700; }
QLabel[role="bigprice"] { color: {accent}; font-size: 26px; font-weight: 800; }
QLabel[role="statvalue"] { font-size: 24px; font-weight: 800; }
QLabel[role="error"] { color: {danger}; }
QLabel[role="hint"] { color: {muted}; background: {surfaceAlt}; border: 1px dashed {border}; border-radius: 8px; padding: 8px 10px; }

/* ---------- badges ---------- */
QLabel[badge] { border-radius: 9px; padding: 2px 9px; font-size: 11px; font-weight: 700; }
QLabel[badge="success"] { background: {successSoft}; color: {success}; }
QLabel[badge="warning"] { background: {warningSoft}; color: {warning}; }
QLabel[badge="danger"]  { background: {dangerSoft}; color: {danger}; }
QLabel[badge="info"]    { background: {infoSoft}; color: {info}; }
QLabel[badge="neutral"] { background: {surfaceAlt}; color: {muted}; }
QLabel[badge="accent"]  { background: {accentSoft}; color: {accent}; }

/* ---------- surfaces ---------- */
QFrame#Card, QFrame#ProductCard { background: {surface}; border: 1px solid {border}; border-radius: 12px; }
QFrame#ProductCard:hover { border: 1px solid {accent}; }
QFrame#Card QLabel, QFrame#ProductCard QLabel { border: none; }
QFrame#TopBar { background: {surface}; border: none; border-bottom: 1px solid {border}; }
QFrame#Divider { background: {border}; border: none; max-height: 1px; min-height: 1px; }
QFrame#Hero, QFrame#BrandPanel {
    border: none; border-radius: 16px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {accent}, stop:1 {heroEnd});
}
QFrame#BrandPanel { border-radius: 0px; }
QFrame#Hero QLabel, QFrame#BrandPanel QLabel { color: #FFFFFF; }
QFrame#Hero QPushButton, QFrame#BrandPanel QPushButton {
    background: #FFFFFF; color: #0B3B33; border: none; border-radius: 8px; padding: 9px 18px; font-weight: 700;
}

/* ---------- sidebar ---------- */
QFrame#Sidebar { background: {sidebar}; border: none; }
QFrame#Sidebar QLabel { color: {sidebarText}; }
QFrame#Sidebar QLabel[badge] { background: {accent}; color: {accentText}; }
QLabel#Logo { color: #FFFFFF; font-size: 22px; font-weight: 800; }
QLabel[sidebar="section"] { color: {sidebarMuted}; font-size: 10px; font-weight: 700; }
QPushButton[nav="true"] {
    background: transparent; color: {sidebarText}; border: none; border-radius: 8px;
    text-align: left; padding: 9px 14px; font-weight: 600;
}
QPushButton[nav="true"]:hover { background: {sidebarHover}; color: #FFFFFF; }
QPushButton[nav="true"]:checked { background: {sidebarActive}; color: #FFFFFF; }
QFrame#UserChip { background: {sidebarHover}; border-radius: 10px; }
QLabel#Avatar { background: {accent}; color: {accentText}; border-radius: 17px; font-weight: 800; }

/* ---------- buttons ---------- */
QPushButton {
    background: {surface}; border: 1px solid {border}; border-radius: 8px;
    padding: 7px 14px; font-weight: 600; color: {text};
}
QPushButton:hover { border-color: {accent}; color: {accent}; }
QPushButton:pressed { background: {surfaceAlt}; }
QPushButton:disabled { color: {muted}; background: {surfaceAlt}; border-color: {border}; }
QPushButton:focus { outline: none; border-color: {accent}; }
QPushButton[variant="primary"] { background: {accent}; color: {accentText}; border: 1px solid {accent}; }
QPushButton[variant="primary"]:hover { background: {accentHover}; border-color: {accentHover}; color: {accentText}; }
QPushButton[variant="primary"]:disabled { background: {accentSoft}; border-color: {accentSoft}; color: {muted}; }
QPushButton[variant="danger"] { background: transparent; color: {danger}; border: 1px solid {danger}; }
QPushButton[variant="danger"]:hover { background: {dangerSoft}; color: {danger}; }
QPushButton[variant="ghost"] { background: transparent; border: 1px solid transparent; color: {muted}; }
QPushButton[variant="ghost"]:hover { color: {accent}; }
QPushButton[variant="link"] { background: transparent; border: none; color: {accent}; padding: 2px; }
QPushButton[variant="chip"] { border-radius: 13px; padding: 6px 14px; }
QPushButton[variant="chip"]:checked { background: {accentSoft}; color: {accent}; border-color: {accent}; }
QPushButton[variant="topbar"] { background: {surfaceAlt}; border: 1px solid {border}; border-radius: 14px; padding: 7px 14px; }
QPushButton[variant="topbar"]:hover { border-color: {accent}; }
QPushButton::menu-indicator { width: 0px; image: none; }

/* ---------- inputs ---------- */
QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit {
    background: {surface}; border: 1px solid {border}; border-radius: 8px; padding: 6px 10px;
    selection-background-color: {accent}; selection-color: {accentText};
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid {accent}; }
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled { color: {muted}; background: {surfaceAlt}; }
QLineEdit#SearchBox { border-radius: 14px; padding: 6px 14px; background: {surfaceAlt}; }
QLineEdit#SearchBox:focus { background: {surface}; }
QComboBox QAbstractItemView {
    background: {surface}; border: 1px solid {border}; outline: 0;
    selection-background-color: {accentSoft}; selection-color: {text};
}
QComboBox { padding-right: 30px; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 28px; border: none; background: transparent; }
QComboBox::down-arrow { image: url("{asset:chevron-down}"); width: 12px; height: 12px; }
QSpinBox { padding-right: 26px; }
QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: border; width: 24px; border: none; background: transparent; }
QSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 8px; }
QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 8px; }
QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: {surfaceAlt}; }
QSpinBox::up-arrow { image: url("{asset:chevron-up}"); width: 10px; height: 10px; }
QSpinBox::down-arrow { image: url("{asset:chevron-down}"); width: 10px; height: 10px; }

QCheckBox, QRadioButton { spacing: 8px; background: transparent; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
QCheckBox::indicator { border: 1px solid {borderStrong}; border-radius: 4px; background: {surface}; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: {accent}; }
QCheckBox::indicator:checked { background: {accent}; border-color: {accent}; image: url("{asset:check}"); }
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { background: {surfaceAlt}; border-color: {border}; }
QRadioButton::indicator { border: 1px solid {borderStrong}; border-radius: 9px; background: {surface}; }
QRadioButton::indicator:checked { border: 5px solid {accent}; width: 8px; height: 8px; background: {surface}; }

/* ---------- tables & lists ---------- */
QTableView, QListWidget, QTreeView {
    background: {surface}; alternate-background-color: {surfaceAlt}; border: 1px solid {border};
    border-radius: 10px; gridline-color: transparent; outline: 0;
    selection-background-color: {accentSoft}; selection-color: {text};
}
QTableView::item, QListWidget::item { padding: 6px 8px; border: none; }
QListWidget::item:selected { background: {accentSoft}; color: {text}; }
QHeaderView { background: transparent; border: none; }
QHeaderView::section {
    background: {surfaceAlt}; color: {muted}; border: none; border-bottom: 1px solid {border};
    padding: 8px 10px; font-weight: 700; font-size: 11px;
}
QTableCornerButton::section { background: {surfaceAlt}; border: none; }

/* ---------- menus ---------- */
QMenu { background: {surface}; border: 1px solid {border}; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 22px 7px 14px; border-radius: 6px; }
QMenu::item:selected { background: {accentSoft}; color: {text}; }
QMenu::separator { height: 1px; background: {border}; margin: 5px 8px; }

/* ---------- scrollbars ---------- */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: {border}; border-radius: 3px; min-height: 30px; }
QScrollBar::handle:horizontal { background: {border}; border-radius: 3px; min-width: 30px; }
QScrollBar::handle:hover { background: {muted}; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QLabel#Toast { background: {text}; color: {bg}; border-radius: 10px; padding: 10px 18px; font-weight: 600; }
QMessageBox QLabel { min-width: 260px; }
)QSS";

// Style sheets can only show arrows and check marks as image files, so we
// draw them for the current colours and save them (1x and @2x) to a temp folder.
QString writeAssets(const ThemePalette &p, const QString &tag, QMap<QString, QString> &paths)
{
    static std::unique_ptr<QTemporaryDir> dir;
    if (!dir) {
        dir = std::make_unique<QTemporaryDir>(QDir::temp().filePath(QStringLiteral("shophub-ui-XXXXXX")));
        if (!dir->isValid())
            return QString();
    }
    auto save = [&](const QString &name, const QColor &color, qreal penWidth, const QVector<QPointF> &points) {
        for (int scale : {1, 2}) {
            const int size = 12 * scale;
            QPixmap pm(size, size);
            pm.fill(Qt::transparent);
            QPainter painter(&pm);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(color, penWidth * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath path;
            path.moveTo(points[0] * size);
            for (int i = 1; i < points.size(); ++i)
                path.lineTo(points[i] * size);
            painter.drawPath(path);
            painter.end();
            const QString file = dir->filePath(QStringLiteral("%1-%2%3.png").arg(name, tag, scale == 2 ? QStringLiteral("@2x") : QString()));
            pm.save(file);
            if (scale == 1)
                paths.insert(name, QDir::fromNativeSeparators(file));
        }
    };
    save(QStringLiteral("chevron-down"), p.muted, 1.6, {{0.22, 0.38}, {0.5, 0.66}, {0.78, 0.38}});
    save(QStringLiteral("chevron-up"), p.muted, 1.6, {{0.22, 0.62}, {0.5, 0.34}, {0.78, 0.62}});
    save(QStringLiteral("check"), p.accentText, 1.8, {{0.2, 0.52}, {0.42, 0.74}, {0.8, 0.3}});
    return dir->path();
}

QString buildStyleSheet(const ThemePalette &p, const QString &tag)
{
    const QMap<QString, QColor> tokens = {
        {QStringLiteral("bg"), p.bg},
        {QStringLiteral("surface"), p.surface},
        {QStringLiteral("surfaceAlt"), p.surfaceAlt},
        {QStringLiteral("border"), p.border},
        {QStringLiteral("borderStrong"), p.borderStrong},
        {QStringLiteral("text"), p.text},
        {QStringLiteral("muted"), p.muted},
        {QStringLiteral("accent"), p.accent},
        {QStringLiteral("accentHover"), p.accentHover},
        {QStringLiteral("accentSoft"), p.accentSoft},
        {QStringLiteral("accentText"), p.accentText},
        {QStringLiteral("heroEnd"), p.heroEnd},
        {QStringLiteral("danger"), p.danger},
        {QStringLiteral("dangerSoft"), p.dangerSoft},
        {QStringLiteral("warning"), p.warning},
        {QStringLiteral("warningSoft"), p.warningSoft},
        {QStringLiteral("success"), p.success},
        {QStringLiteral("successSoft"), p.successSoft},
        {QStringLiteral("info"), p.info},
        {QStringLiteral("infoSoft"), p.infoSoft},
        {QStringLiteral("sidebar"), p.sidebar},
        {QStringLiteral("sidebarHover"), p.sidebarHover},
        {QStringLiteral("sidebarActive"), p.sidebarActive},
        {QStringLiteral("sidebarText"), p.sidebarText},
        {QStringLiteral("sidebarMuted"), p.sidebarMuted},
    };
    QString qss = QString::fromUtf8(kStyleSheet);
    for (auto it = tokens.cbegin(); it != tokens.cend(); ++it)
        qss.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value().name());

    QMap<QString, QString> assets;
    writeAssets(p, tag, assets);
    for (auto it = assets.cbegin(); it != assets.cend(); ++it)
        qss.replace(QStringLiteral("{asset:") + it.key() + QLatin1Char('}'), it.value());
    return qss;
}

QPalette buildQPalette(const ThemePalette &p)
{
    QPalette pal;
    pal.setColor(QPalette::Window, p.bg);
    pal.setColor(QPalette::WindowText, p.text);
    pal.setColor(QPalette::Base, p.surface);
    pal.setColor(QPalette::AlternateBase, p.surfaceAlt);
    pal.setColor(QPalette::Text, p.text);
    pal.setColor(QPalette::Button, p.surface);
    pal.setColor(QPalette::ButtonText, p.text);
    pal.setColor(QPalette::Highlight, p.accent);
    pal.setColor(QPalette::HighlightedText, p.accentText);
    pal.setColor(QPalette::PlaceholderText, p.muted);
    pal.setColor(QPalette::ToolTipBase, p.text);
    pal.setColor(QPalette::ToolTipText, p.surface);
    pal.setColor(QPalette::Link, p.accent);
    pal.setColor(QPalette::Light, p.surface);
    pal.setColor(QPalette::Midlight, p.surfaceAlt);
    pal.setColor(QPalette::Mid, p.border);
    pal.setColor(QPalette::Dark, p.border.darker(130));
    pal.setColor(QPalette::Shadow, QColor(0, 0, 0, 80));
    pal.setColor(QPalette::Disabled, QPalette::Text, p.muted);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, p.muted);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, p.muted);
    return pal;
}

} // namespace

void Theme::apply(ThemeMode mode)
{
    g_mode = mode;
    g_palette = mode == ThemeMode::Dark ? darkPalette() : lightPalette();
    // Fusion looks the same on every platform and respects the palette fully.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(buildQPalette(g_palette));
    qApp->setStyleSheet(buildStyleSheet(g_palette, mode == ThemeMode::Dark ? QStringLiteral("dark") : QStringLiteral("light")));
}

ThemeMode Theme::current()
{
    return g_mode;
}

const ThemePalette &Theme::palette()
{
    return g_palette;
}

ThemeMode Theme::loadSaved()
{
    QSettings s;
    return s.value(QStringLiteral("ui/theme")).toString() == QLatin1String("dark") ? ThemeMode::Dark : ThemeMode::Light;
}

void Theme::save(ThemeMode mode)
{
    QSettings s;
    s.setValue(QStringLiteral("ui/theme"), mode == ThemeMode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
}

QString Theme::badgeKind(OrderStatus status)
{
    switch (status) {
    case OrderStatus::Pending: return QStringLiteral("warning");
    case OrderStatus::Confirmed:
    case OrderStatus::Processing: return QStringLiteral("info");
    case OrderStatus::Shipped: return QStringLiteral("accent");
    case OrderStatus::Delivered: return QStringLiteral("success");
    case OrderStatus::Cancelled: return QStringLiteral("danger");
    }
    return QStringLiteral("neutral");
}

QColor Theme::statusColor(OrderStatus status)
{
    const ThemePalette &p = g_palette;
    switch (status) {
    case OrderStatus::Pending: return p.warning;
    case OrderStatus::Confirmed:
    case OrderStatus::Processing: return p.info;
    case OrderStatus::Shipped: return p.accent;
    case OrderStatus::Delivered: return p.success;
    case OrderStatus::Cancelled: return p.danger;
    }
    return p.muted;
}

} // namespace shophub::ui
