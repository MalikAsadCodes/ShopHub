#pragma once

#include "domain/Types.h"

#include <QWidget>

class QGridLayout;

namespace shophub::ui {

class ProductCard;

// Lays product cards out in as many columns as fit, re-flowing on resize.
class ProductGrid : public QWidget
{
    Q_OBJECT
public:
    explicit ProductGrid(QWidget *parent = nullptr);

    void clear();
    void addCard(ProductCard *card);
    int count() const { return m_cards.size(); }
    void setMaxColumns(int columns) { m_maxColumns = columns; relayout(); }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    int columnsForWidth(int width) const;

    QGridLayout *m_layout;
    QList<ProductCard *> m_cards;
    int m_columns = 0;
    int m_maxColumns = 0; // 0 = unlimited
};

} // namespace shophub::ui
