#include "texteditorstyle.h"
#include "texteditorstyle_p.h"

#include <QGuiApplication>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>

namespace TextEditorStyle {

const QColor &linkColor()
{
    static QColor color(LINK_COLOR);
    return color;
}

const QString &listPadding()
{
    static QString padding(QStringLiteral(LIST_PADDING));
    return padding;
}

const int &listPaddingLength()
{
    static const int length = listPadding().length();
    return length;
}

/*
 * Sets up properties for a QTextBlockFormat that is used each time
 * a new block is added to a QTextDocument in TextEditor.
 */
const QTextBlockFormat &defaultBlockFormat()
{
    // Define aliases
    using LineHeightType = QTextBlockFormat::LineHeightTypes;

    static QTextBlockFormat blockFmt;
    static bool init = false;

    if (init)
        return blockFmt;

    // Apply styles to blockFmt

#ifdef BLOCK_LINE_HEIGHT
    const QString lineHeight = QStringLiteral(BLOCK_LINE_HEIGHT);
    QString valueStr = lineHeight.trimmed();
    if (valueStr.endsWith(QStringLiteral("%"))) {
        while (valueStr.endsWith(QLatin1Char('%')) || valueStr.endsWith(QLatin1Char(' ')))
            valueStr.chop(1);

        bool ok = false;
        const double floatVal = valueStr.toDouble(&ok);

        if (ok) {
            blockFmt.setProperty(QTextFormat::LineHeightType, LineHeightType::ProportionalHeight);
            blockFmt.setProperty(QTextFormat::LineHeight, floatVal);
        }
    } else {
        bool ok = false;
        const double floatVal = valueStr.toDouble(&ok);

        if (ok) {
            blockFmt.setProperty(QTextFormat::LineHeightType, LineHeightType::FixedHeight);
            blockFmt.setProperty(QTextFormat::LineHeight, floatVal);
        }
    }
#endif

#ifdef BLOCK_TOP_MARGIN
    blockFmt.setProperty(QTextFormat::BlockTopMargin, (float)BLOCK_TOP_MARGIN);
#endif

#ifdef BLOCK_BOTTOM_MARGIN
    blockFmt.setProperty(QTextFormat::BlockBottomMargin, (float)BLOCK_BOTTOM_MARGIN);
#endif

    init = true;
    return blockFmt;
}

const QTextLength &horizontalRulerWidth()
{
    static QTextLength hrw;
    static bool init = false;

    if (init)
        return hrw;

#ifdef HORIZONTAL_RULER_WIDTH
    const QString widthStr = QStringLiteral(HORIZONTAL_RULER_WIDTH);
    QString valueStr = widthStr.trimmed();
    if (valueStr.endsWith(QStringLiteral("%"))) {
        while (valueStr.endsWith(QLatin1Char('%')) || valueStr.endsWith(QLatin1Char(' ')))
            valueStr.chop(1);

        bool ok = false;
        const double floatVal = valueStr.toDouble(&ok);

        if (ok)
            hrw = QTextLength(QTextLength::PercentageLength, floatVal);
    } else {
        bool ok = false;
        const double floatVal = valueStr.toDouble(&ok);

        if (ok)
            hrw = QTextLength(QTextLength::FixedLength, floatVal);
    }
#endif

    init = true;
    return hrw;
}

const QColor &horizontalRulerColor()
{
    static QColor hrc;
    static bool init = false;

    if (init)
        return hrc;

#ifdef HORIZONTAL_RULER_COLOR
    hrc = QColor(HORIZONTAL_RULER_COLOR);
#else
    hrc = QColor::Invalid;
#endif

    init = true;
    return hrc;
}

const QTextCharFormat &defaultCharFormat()
{
    static QTextCharFormat charFmt;
    static bool init = false;

    if (init)
        return charFmt;

    charFmt.setFontPointSize(defaultFontPointSize());
    if (QGuiApplication::instance())
        init = true;

    return charFmt;
}

int defaultFontPointSize()
{
    // Return default fontsize from application, if available
    if (QGuiApplication::instance())
        return QGuiApplication::font().pointSize();
    // Return some pointsize as default
    // It will be used only for test cases, since they cannot use QGuiApplication
    return 13;
}

bool isMarkdownStrong(const QTextCharFormat &charFormat)
{
    return charFormat.fontWeight() >= StrongFontWeight;
}

/*
 * Return the default font weight for the block's semantic type.
 */
QFont::Weight blockDefaultFontWeight(const QTextBlock &block)
{
    if (block.blockFormat().headingLevel() > 0)
        return HeadingFontWeight;
    return NormalFontWeight;
}

} // namespace TextEditorStyle
