#include "texteditor/widgets/toolbarseparator.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

namespace notetree::texteditor {

ToolBarSeparator::ToolBarSeparator(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(5);
}

void ToolBarSeparator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect rect = this->rect().adjusted(0, 0, -1, -1);
    painter.fillRect(rect, QColor(QStringLiteral("#efefef")));

    QPen pen(Qt::DotLine);
    pen.setColor(QColor(QStringLiteral("#d7d7d7")));
    painter.setPen(pen);

    QPainterPath left;
    left.addRect(QRectF(rect.adjusted(0, 0, -4, 0)));
    painter.drawPath(left);

    QPainterPath right;
    right.addRect(QRectF(rect.adjusted(4, 0, 0, 0)));
    painter.drawPath(right);
}

} // namespace notetree::texteditor
