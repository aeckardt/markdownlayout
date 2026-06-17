#include "gradientbutton.h"

#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

inline constexpr int GradientColors = 5;

QImage toGrayscale(const QImage &image, double brightnessFactor)
{
    const int width = image.width();
    const int height = image.height();

    QImage grayscaleImage(width, height, QImage::Format_ARGB32);

    int x, y;
    QRgb rgb;
    int red, green, blue, alpha;
    int avgBrightness;  // 0..255
    QRgb grayscaleRgb;

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            // Decompose image pixel color
            rgb = image.pixel(x, y);
            red = qRed(rgb);
            green = qGreen(rgb);
            blue = qBlue(rgb);
            alpha = qAlpha(rgb);

            // Set grayscale pixel color from average RGB value
            avgBrightness = int((brightnessFactor * 255 + red + green + blue) / (brightnessFactor + 3));
            grayscaleRgb = alpha * QRgb(0x1000000) + avgBrightness * QRgb(0x010101);
            grayscaleImage.setPixel(x, y, grayscaleRgb);
        }
    }

    return grayscaleImage;
}

QColor adjustBrightness(const QColor &color, int delta)
{
    return QColor(std::clamp<int>(color.red() + delta, 0, 255),
                  std::clamp<int>(color.green() + delta, 0, 255),
                  std::clamp<int>(color.blue() + delta, 0, 255));
}

GradientButton::GradientButton(const QImage &image, bool convertToGrayscale,
                               const QColor &baseColor, const QColor &checkedColor,
                               QWidget *parent)
    : QWidget(parent),
      m_type(Type::ImageButton),
      m_textAlignment(Qt::AlignCenter),
      m_shadow(new QGraphicsDropShadowEffect(this)),
      m_shadowEnabled(false),
      m_state(State::Invalid),
      m_checkable(false),
      m_checked(false),
      m_mousePressed(false),
      m_keyState(0)
{
    if (image.isNull()) {
        qWarning() << QStringLiteral("Expected image is null");
        return;
    }

    setImage(image, convertToGrayscale);
    setGradientColors(baseColor, checkedColor);
    setState(State::Inactive);

    initShadow();
}

GradientButton::GradientButton(const QString &text, Qt::AlignmentFlag textAlignment,
                               const QColor &baseColor, const QColor &checkedColor,
                               QWidget *parent)
    : QWidget(parent),
      m_type(Type::TextButton),
      m_textAlignment(textAlignment),
      m_shadow(new QGraphicsDropShadowEffect(this)),
      m_shadowEnabled(false),
      m_state(State::Invalid),
      m_checkable(false),
      m_checked(false),
      m_mousePressed(false),
      m_keyState(0)
{
    if (!text.isEmpty())
        setText(text);

    setGradientColors(baseColor, checkedColor);
    setState(State::Inactive);

    initShadow();
}

void GradientButton::setGradientColors(const QColor &baseColor, const QColor &checkedColor)
{
    m_defaultColors.clear();
    m_checkedStateColors.clear();
    for (int i = 0; i < GradientColors; ++i) {
        m_defaultColors.append(adjustBrightness(baseColor, i * 16));
        if (checkedColor.isValid())
            m_checkedStateColors.append(adjustBrightness(checkedColor, i * 8));
        else
            m_checkedStateColors.append(m_defaultColors.last());
    }
}

void GradientButton::setImage(const QImage &image, bool convertToGrayscale)
{
    Q_ASSERT(m_type == Type::ImageButton);

    if (m_image.data_ptr() != nullptr &&
            m_image.data_ptr() == const_cast<QImage *>(&image)->data_ptr())
        // Same object or shared data
        // -> Do nothing
        return;

    if (convertToGrayscale)
        m_image = toGrayscale(image, 0.5);
    else
        m_image = image;
    m_disabledImage = toGrayscale(image, 5);
    m_imagePos = calcImagePos();

    update();
}

void GradientButton::setText(const QString &text)
{
    Q_ASSERT(m_type == Type::TextButton);

    QFontMetrics fm(font());
    m_labelRect = fm.boundingRect(text);
    m_text = text;
    m_textPos = calcTextPos();

    update();
}

void GradientButton::setFont(const QFont &font)
{
    Q_ASSERT(m_type == Type::TextButton);

    QFontMetrics fm(font);
    m_labelRect = fm.boundingRect(m_text);

    QWidget::setFont(font);
}

void GradientButton::setCheckable(bool checkable)
{
    if (m_checkable == checkable)
        return;
    m_checkable = checkable;
    updateState();
}

void GradientButton::setChecked(bool checked)
{
    if (m_checked != checked) {
        m_checked = checked;
        updateState();
    }
}

void GradientButton::setShadowEnabled(bool enable)
{
    if (m_shadowEnabled != enable) {
        m_shadowEnabled = enable;
        updateShadowState();
    }
}

QSize GradientButton::minimumSizeHint() const
{
    if (m_type == Type::ImageButton)
        return m_image.size() + QSize(6, 6);
    else if (!m_text.isEmpty())
        return m_labelRect.size() + QSize(12, 6);
    return QSize(1, 1);
}

QSize GradientButton::sizeHint() const
{
    if (m_type == Type::ImageButton)
        return m_image.size() + QSize(12, 10);
    else if (!m_text.isEmpty())
        return m_labelRect.size() + QSize(12, 6);
    return QSize(1, 1);
}

void GradientButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QRect r = rect().adjusted(0, 0, -1, -1);
    painter.fillRect(r, m_gradient);

    QPainterPath path;
    path.addRect(QRectF(r));

    painter.setPen(QColor("#dfdfdf"));
    painter.drawPath(path);

    if (m_type == Type::ImageButton) {
        if (m_state != State::Disabled)
            painter.drawImage(m_imagePos, m_image);
        else
            painter.drawImage(m_imagePos, m_disabledImage);
    } else if (!m_text.isEmpty()) {
        painter.setFont(font());
        painter.setPen(m_textColor);
        painter.drawText(m_textPos.x(), m_textPos.y() + font().pointSize(), m_text);
    }
}

void GradientButton::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    m_gradient.setStart(0, 0);
    m_gradient.setFinalStop(0, height());

    if (m_type == Type::ImageButton)
        m_imagePos = calcImagePos();
    else
        m_textPos = calcTextPos();
}

void GradientButton::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);
    if (event->key() == Qt::Key_Space) {
        m_keyState |= event->key();
        updateState();
        emit clicked();
    } else if (event->key() == Qt::Key_Tab) {
        m_keyState = 0;
        updateState();
    }
}

void GradientButton::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Space) {
        m_keyState &= ~event->key();
        updateState();
    }
}

void GradientButton::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        m_mousePressed = true;
        updateState();
    }
}

void GradientButton::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        m_mousePressed = false;
        if (rect().contains(event->pos())) {
            if (m_checkable)
                m_checked = !m_checked;
            emit clicked();
        }
        updateState();
    }
}

void GradientButton::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange)
        updateState();
    QWidget::changeEvent(event);
}

void GradientButton::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    updateState();
}

void GradientButton::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    updateState();
}

void GradientButton::initShadow()
{
    m_shadow->setBlurRadius(15);
    m_shadow->setOffset(1, 0);
    m_shadow->setColor(QColor("#8eb7f1"));
    setGraphicsEffect(m_shadow);
}

void GradientButton::setState(State newState)
{
    // TODO: Think about the design of this class
    // It is a little odd that there is no update in this function
    // It's a least confusing that setState doesn't call updateState
    // You might think about renaming the functions or changing the overall design
    // to make it more comprehensible

    if (newState == m_state)
        return;

    switch (newState) {
    case State::Disabled:
        m_gradient.setColorAt(0,    QColor("#d7d7d7"));
        m_gradient.setColorAt(0.15, QColor("#dfdfdf"));
        m_gradient.setColorAt(0,    QColor("#e7e7e7"));
        m_gradient.setColorAt(0.85, QColor("#efefef"));
        m_gradient.setColorAt(0,    QColor("#f3f3f3"));
        m_offset = 0;
        m_textColor = QColor("#555555");
        break;
    case State::Inactive:
    case State::Active:
    case State::Clicked:
        m_gradient.setColorAt(0,    m_defaultColors.at(0));
        m_gradient.setColorAt(0.15, m_defaultColors.at(1));
        m_gradient.setColorAt(0,    m_defaultColors.at(2));
        m_gradient.setColorAt(0.85, m_defaultColors.at(3));
        m_gradient.setColorAt(0,    m_defaultColors.at(4));
        m_offset = newState == State::Clicked ? 1 : 0;
        m_textColor = QColor("#111111");
        break;
    case State::Checked:
        m_gradient.setColorAt(0,    m_checkedStateColors.at(0));
        m_gradient.setColorAt(0.15, m_checkedStateColors.at(1));
        m_gradient.setColorAt(0,    m_checkedStateColors.at(2));
        m_gradient.setColorAt(0.85, m_checkedStateColors.at(3));
        m_gradient.setColorAt(0,    m_checkedStateColors.at(4));
        m_offset = 1;
        m_textColor = QColor("#111111");
        break;
    default:
        ;
    }

    if (m_type == Type::ImageButton)
        m_imagePos = calcImagePos();
    else
        m_textPos = calcTextPos();

    m_state = newState;

    updateShadowState();
}

void GradientButton::updateShadowState()
{
    if (m_state == State::Inactive || m_state == State::Disabled)
        m_shadow->setEnabled(false);
    else
        m_shadow->setEnabled(m_shadowEnabled);
}

/*
 * Determine the current state of the button based on its context.
 */
void GradientButton::updateState()
{
    // TODO: Sanitize this function
    // It seems a little odd that states are set like this
    // depending on other state variables

    State oldState = m_state;

    if (!isEnabled())
        setState(State::Disabled);
    else if (m_mousePressed || m_keyState != 0)
        // Set clicked state if mouse button or Space is pressed
        setState(State::Clicked);
    else if (m_checkable && m_checked)
        setState(State::Checked);
    else if (hasFocus())
        setState(State::Active);
    else
        setState(State::Inactive);

    if (oldState != m_state)
        update();
}

QPoint GradientButton::calcImagePos() const
{
    QPoint offset(m_offset, m_offset);
    return rect().center() - m_image.rect().center() + offset;
}

QPoint GradientButton::calcTextPos() const
{
    QPoint offset(m_offset, m_offset);
    QPoint topLeft = rect().center() - m_labelRect.center() - QPoint(0, font().pointSize());
    switch (m_textAlignment) {
    case Qt::AlignCenter:
        return topLeft + offset;
    case Qt::AlignLeft:
        return QPoint(0, topLeft.y()) + offset;
    default:
        // Other alignments are not yet implemented
        // Revert to default (center)
        return topLeft + offset;
    }
}
