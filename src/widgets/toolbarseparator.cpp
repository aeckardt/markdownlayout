#include "toolbarseparator.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRect>

ToolBarSeparator::ToolBarSeparator(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(5);
}

void ToolBarSeparator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QRect r = rect().adjusted(0, 0, -1, -1);
    painter.fillRect(r, QColor("#efefef"));

    QPen pen(Qt::DotLine);
    pen.setColor(QColor("#d7d7d7"));
    painter.setPen(pen);

    QRect _r = r.adjusted(0, 0, -4, 0);
    QPainterPath path;
    path.addRect(_r.toRectF());
    painter.drawPath(path);

    _r = r.adjusted(4, 0, 0, 0);
    path.clear();
    path.addRect(_r.toRectF());
    painter.drawPath(path);
}
