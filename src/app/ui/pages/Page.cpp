#include "ui/pages/Page.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

namespace shophub::ui {

Page::Page(UiContext &ui, QWidget *parent)
    : QWidget(parent), m_ui(ui), m_layout(new QVBoxLayout(this))
{
    m_layout->setContentsMargins(28, 22, 28, 22);
    m_layout->setSpacing(16);
}

void Page::addHeader(const QString &title, const QString &subtitle, QWidget *trailing)
{
    auto *row = new QHBoxLayout();
    auto *texts = new QVBoxLayout();
    texts->setSpacing(2);
    texts->addWidget(makeLabel(title, "h1"));
    if (!subtitle.isEmpty())
        texts->addWidget(makeLabel(subtitle, "muted"));
    row->addLayout(texts);
    row->addStretch();
    if (trailing)
        row->addWidget(trailing, 0, Qt::AlignBottom);
    m_layout->addLayout(row);
}

QScrollArea *Page::scrollWrap(QWidget *content)
{
    content->setObjectName(QStringLiteral("ScrollContent"));
    auto *area = new QScrollArea();
    area->setWidget(content);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return area;
}

} // namespace shophub::ui
