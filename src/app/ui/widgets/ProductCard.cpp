#include "ui/widgets/ProductCard.h"

#include "ui/UiCommon.h"
#include "ui/widgets/ImageView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace shophub::ui {

ProductCard::ProductCard(const Product &product, const QString &imagePath, bool canBuy, QWidget *parent)
    : QFrame(parent), m_productId(product.id)
{
    setObjectName(QStringLiteral("ProductCard"));
    setFixedWidth(kWidth);
    setCursor(Qt::PointingHandCursor);
    setToolTip(product.name);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 12);
    layout->setSpacing(0);

    auto *image = new ImageView(this);
    image->setFixedHeight(150);
    image->setRadius(11);
    image->setTopCornersOnly(true);
    image->setImage(imagePath, product.name, product.categoryName);
    layout->addWidget(image);

    auto *body = new QVBoxLayout();
    body->setContentsMargins(13, 10, 13, 0);
    body->setSpacing(4);

    // Category on the left; a wholesale badge on the same line keeps every card the same height.
    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(makeLabel(product.categoryName.toUpper(), "small"));
    topRow->addStretch();
    if (product.wholesaleFrom > 0) {
        auto *wholesale = makeBadge(QStringLiteral("Wholesale %1+").arg(product.wholesaleFrom), QStringLiteral("accent"));
        wholesale->setToolTip(QStringLiteral("Lower price when you buy %1 or more").arg(product.wholesaleFrom));
        topRow->addWidget(wholesale);
    }
    body->addLayout(topRow);

    auto *name = makeLabel(QString(), "h3");
    name->setWordWrap(true);
    name->setFixedHeight(40);
    name->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    const QString shown = fontMetrics().elidedText(product.name, Qt::ElideRight, (kWidth - 30) * 2 - 20);
    name->setText(shown);
    body->addWidget(name);

    if (!product.brand.isEmpty())
        body->addWidget(makeLabel(product.brand, "muted"));

    auto *priceRow = new QHBoxLayout();
    priceRow->setContentsMargins(0, 4, 0, 0);
    priceRow->addWidget(makeLabel(product.price.toString(), "price"));
    priceRow->addStretch();
    priceRow->addWidget(makeBadge(stockText(product.stock, product.lowStockThreshold),
                                  stockBadgeKind(product.stock, product.lowStockThreshold)));
    body->addLayout(priceRow);

    if (canBuy) {
        auto *add = makeButton(product.inStock() ? QStringLiteral("Add to cart") : QStringLiteral("Out of stock"),
                               "primary");
        add->setEnabled(product.inStock());
        add->setAccessibleName(QStringLiteral("Add %1 to cart").arg(product.name));
        connect(add, &QPushButton::clicked, this, [this] { emit addToCartRequested(m_productId); });
        body->addSpacing(6);
        body->addWidget(add);
    }

    layout->addLayout(body);
}

void ProductCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit openRequested(m_productId);
    QFrame::mouseReleaseEvent(event);
}

} // namespace shophub::ui
