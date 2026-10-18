#include "ui/widgets/ProductGrid.h"

#include "ui/widgets/ProductCard.h"

#include <QGridLayout>
#include <QResizeEvent>

namespace shophub::ui {

namespace {
constexpr int kSpacing = 16;
}

ProductGrid::ProductGrid(QWidget *parent)
    : QWidget(parent), m_layout(new QGridLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setHorizontalSpacing(kSpacing);
    m_layout->setVerticalSpacing(kSpacing);
    m_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

void ProductGrid::clear()
{
    for (ProductCard *card : std::as_const(m_cards)) {
        m_layout->removeWidget(card);
        card->deleteLater();
    }
    m_cards.clear();
    m_columns = 0;
}

void ProductGrid::addCard(ProductCard *card)
{
    card->setParent(this);
    m_cards.append(card);
    const int cols = qMax(1, columnsForWidth(width()));
    const int index = m_cards.size() - 1;
    if (cols != m_columns && m_columns != 0) {
        relayout();
    } else {
        m_columns = cols;
        m_layout->addWidget(card, index / cols, index % cols);
    }
    card->show();
}

int ProductGrid::columnsForWidth(int width) const
{
    const int cols = qMax(1, (width + kSpacing) / (ProductCard::kWidth + kSpacing));
    return m_maxColumns > 0 ? qMin(cols, m_maxColumns) : cols;
}

void ProductGrid::relayout()
{
    const int cols = columnsForWidth(width());
    for (ProductCard *card : std::as_const(m_cards))
        m_layout->removeWidget(card);
    for (int i = 0; i < m_cards.size(); ++i)
        m_layout->addWidget(m_cards[i], i / cols, i % cols);
    m_columns = cols;
}

void ProductGrid::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (columnsForWidth(event->size().width()) != m_columns)
        relayout();
}

} // namespace shophub::ui
