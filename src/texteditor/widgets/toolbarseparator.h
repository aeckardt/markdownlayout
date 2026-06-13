#pragma once

#include <QWidget>

namespace notetree::texteditor {

class ToolBarSeparator : public QWidget {
    Q_OBJECT
public:
    explicit ToolBarSeparator(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

} // namespace notetree::texteditor
