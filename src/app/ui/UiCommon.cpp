#include "ui/UiCommon.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLayout>
#include <QTableView>
#include <QLocale>
#include <QMessageBox>
#include <QStyle>

namespace shophub::ui {

QLabel *makeLabel(const QString &text, const char *role, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    if (role)
        l->setProperty("role", QString::fromLatin1(role));
    return l;
}

QPushButton *makeButton(const QString &text, const char *variant, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    if (variant)
        b->setProperty("variant", QString::fromLatin1(variant));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QLabel *makeBadge(const QString &text, const QString &kind, QWidget *parent)
{
    auto *l = new QLabel(parent);
    setBadge(l, text, kind);
    l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return l;
}

void setBadge(QLabel *label, const QString &text, const QString &kind)
{
    label->setText(text);
    label->setProperty("badge", kind);
    repolish(label);
}

QFrame *makeCard(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("Card"));
    return f;
}

QFrame *makeDivider(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("Divider"));
    f->setFrameShape(QFrame::NoFrame);
    return f;
}

QLabel *makeFormLabel(const QString &text)
{
    auto *l = makeLabel(text, "muted");
    l->setMinimumHeight(34);
    l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return l;
}

QString escapeMnemonic(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        if (QLayout *child = item->layout())
            clearLayout(child);
        delete item;
    }
}

void setupTable(QTableView *table)
{
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setSortingEnabled(true);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(38);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setMinimumSectionSize(60);
}

void showError(QWidget *parent, const QString &message)
{
    QMessageBox box(QMessageBox::Warning, QStringLiteral("ShopHub"), message, QMessageBox::Ok, parent);
    box.exec();
}

void showInfo(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox box(QMessageBox::Information, title, message, QMessageBox::Ok, parent);
    box.exec();
}

bool confirm(QWidget *parent, const QString &title, const QString &message, const QString &okText)
{
    QMessageBox box(QMessageBox::Question, title, message, QMessageBox::NoButton, parent);
    QPushButton *ok = box.addButton(okText, QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(ok);
    box.exec();
    return box.clickedButton() == ok;
}

QString formatDateTime(const QDateTime &utc)
{
    if (!utc.isValid())
        return QString();
    return QLocale(QLocale::English).toString(utc.toLocalTime(), QStringLiteral("d MMM yyyy, h:mm AP"));
}

QString formatDate(const QDateTime &utc)
{
    if (!utc.isValid())
        return QString();
    return QLocale(QLocale::English).toString(utc.toLocalTime(), QStringLiteral("d MMM yyyy"));
}

QString initials(const QString &name)
{
    const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString out;
    for (const QString &p : parts) {
        if (p[0].isLetterOrNumber())
            out.append(p[0].toUpper());
        if (out.size() == 2)
            break;
    }
    return out.isEmpty() ? QStringLiteral("?") : out;
}

QString stockText(int stock, int lowThreshold)
{
    if (stock <= 0)
        return QStringLiteral("Out of stock");
    if (stock <= lowThreshold)
        return QStringLiteral("Only %1 left").arg(stock);
    return QStringLiteral("In stock");
}

QString stockBadgeKind(int stock, int lowThreshold)
{
    if (stock <= 0)
        return QStringLiteral("danger");
    if (stock <= lowThreshold)
        return QStringLiteral("warning");
    return QStringLiteral("success");
}

} // namespace shophub::ui
