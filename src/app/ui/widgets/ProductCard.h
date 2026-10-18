#pragma once

#include "domain/Types.h"

#include <QFrame>

class QLabel;
class QPushButton;

namespace shophub::ui {

class ImageView;

// One product tile in a grid. Click anywhere to open details.
class ProductCard : public QFrame
{
    Q_OBJECT
public:
    static constexpr int kWidth = 228;

    ProductCard(const Product &product, const QString &imagePath, bool canBuy, QWidget *parent = nullptr);
    qint64 productId() const { return m_productId; }

signals:
    void openRequested(qint64 productId);
    void addToCartRequested(qint64 productId);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qint64 m_productId = 0;
};

} // namespace shophub::ui
