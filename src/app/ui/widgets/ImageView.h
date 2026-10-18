#pragma once

#include <QPixmap>
#include <QWidget>

namespace shophub::ui {

// Shows a product photo (cropped to fill) or, when there is none, a
// generated placeholder: a colour picked from the category plus initials.
class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);

    void setImage(const QString &absolutePath, const QString &title, const QString &colorSeed);
    void setRadius(int radius) { m_radius = radius; update(); }
    void setTopCornersOnly(bool topOnly) { m_topOnly = topOnly; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
    QString m_initials;
    QString m_seed;
    int m_radius = 10;
    bool m_topOnly = false;
};

} // namespace shophub::ui
