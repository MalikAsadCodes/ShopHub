#include "ui/widgets/ImageView.h"

#include "ui/Theme.h"
#include "ui/UiCommon.h"

#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace shophub::ui {

namespace {

QPixmap loadScaled(const QString &path)
{
    QPixmap pm;
    const QString key = QStringLiteral("img:") + path;
    if (QPixmapCache::find(key, &pm))
        return pm;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // Downscale big photos once; cards and dialogs never need more than this.
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > 900 || size.height() > 900))
        reader.setScaledSize(size.scaled(900, 900, Qt::KeepAspectRatio));
    const QImage img = reader.read();
    if (img.isNull())
        return QPixmap();
    pm = QPixmap::fromImage(img);
    QPixmapCache::insert(key, pm);
    return pm;
}

} // namespace

ImageView::ImageView(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void ImageView::setImage(const QString &absolutePath, const QString &title, const QString &colorSeed)
{
    m_pixmap = absolutePath.isEmpty() || !QFile::exists(absolutePath) ? QPixmap() : loadScaled(absolutePath);
    m_initials = initials(title);
    m_seed = colorSeed.isEmpty() ? title : colorSeed;
    update();
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF r = rect();
    QPainterPath clip;
    if (m_topOnly) {
        clip.addRoundedRect(r.adjusted(0, 0, 0, m_radius), m_radius, m_radius);
        QPainterPath box;
        box.addRect(r);
        clip = clip.intersected(box);
    } else {
        clip.addRoundedRect(r, m_radius, m_radius);
    }
    p.setClipPath(clip);

    if (!m_pixmap.isNull()) {
        p.fillRect(r, Theme::palette().surfaceAlt);
        const QSize scaled = m_pixmap.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
        p.drawPixmap(target, m_pixmap);
        return;
    }

    // Placeholder: a soft two-tone gradient whose hue is stable per category.
    const int hue = static_cast<int>(qHash(m_seed) % 360);
    const bool dark = Theme::current() == ThemeMode::Dark;
    const QColor c1 = QColor::fromHsl(hue, 120, dark ? 70 : 215);
    const QColor c2 = QColor::fromHsl((hue + 40) % 360, 110, dark ? 50 : 190);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0, c1);
    g.setColorAt(1, c2);
    p.fillRect(r, g);

    QColor ink = QColor::fromHsl(hue, 90, dark ? 190 : 70);
    ink.setAlpha(dark ? 200 : 170);
    p.setPen(ink);
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(qMax(14, qMin(width(), height()) / 3));
    p.setFont(f);
    p.drawText(r, Qt::AlignCenter, m_initials);
}

} // namespace shophub::ui
