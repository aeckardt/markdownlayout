#ifndef GRADIENTBUTTON_H
#define GRADIENTBUTTON_H

#include <QColor>
#include <QLinearGradient>
#include <QRect>
#include <QStringLiteral>
#include <QVector>
#include <QWidget>

class QGraphicsDropShadowEffect;

class GradientButton : public QWidget
{
    Q_OBJECT
public:
    enum Type {
        ImageButton,
        TextButton
    };

    // Constructor for ImageButton
    explicit GradientButton(const QImage &image, bool convertToGrayscale = false,
                            const QColor &baseColor = QColor(QStringLiteral("#ebebeb")),
                            const QColor &checkedColor = QColor(),
                            QWidget *parent = nullptr);

    // Constructor for TextButton
    explicit GradientButton(const QString &text, Qt::AlignmentFlag textAlignment = Qt::AlignCenter,
                            const QColor &baseColor = QColor(QStringLiteral("#ebebeb")),
                            const QColor &checkedColor = QColor(),
                            QWidget *parent = nullptr);

    void setGradientColors(const QColor &baseColor, const QColor &checkedColor = QColor());

    void setImage(const QImage &image, bool convertToGrayscale = false);

    void setText(const QString &text);
    void setFont(const QFont &font);

    void setCheckable(bool checkable);
    void setChecked(bool checked);
    bool isChecked() const { return m_checkable && m_checked; }

    void setShadowEnabled(bool enable);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    // Rendering functions
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;

    // Key / mouse input handlers
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    // Other event handlers
    void changeEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool) override { return false; }

private:
    enum State {
        Invalid,
        Disabled,
        Inactive,
        Active,
        Clicked,
        Checked
    };

    void initShadow();

    // State management
    void setState(State newState);
    void updateShadowState();
    void updateState();

    // Rendering helpers
    QPoint calcImagePos() const;
    QPoint calcTextPos() const;

    Type m_type;

    // Image specific properties
    QImage m_image;
    QImage m_disabledImage;
    QPoint m_imagePos;

    // Text specific properties
    QString m_text;
    Qt::AlignmentFlag m_textAlignment;
    QPoint m_textPos;
    QRect m_labelRect;
    QColor m_textColor;

    // Gradient colors
    QVector<QColor> m_defaultColors;
    QVector<QColor> m_checkedStateColors;
    QLinearGradient m_gradient;

    // Rendering variables
    int m_offset;

    // Shadow
    QGraphicsDropShadowEffect *m_shadow;
    bool m_shadowEnabled;

    // Button state
    State m_state;

    bool m_checkable;
    bool m_checked;

    bool m_mousePressed;
    int m_keyState;
};

#endif // GRADIENTBUTTON_H
