#ifndef TOOLBARSEPARATOR_H
#define TOOLBARSEPARATOR_H

#include <QWidget>

class ToolBarSeparator : public QWidget
{
    Q_OBJECT
public:
    explicit ToolBarSeparator(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

#endif // TOOLBARSEPARATOR_H
