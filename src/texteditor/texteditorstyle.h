#ifndef TEXTEDITORSTYLE_H
#define TEXTEDITORSTYLE_H

#include <QColor>
#include <QFont>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextLength>
#include <QTextListFormat>

namespace TextEditorStyle {

// Modifiable style constants

// TextEditor styles
inline constexpr QTextListFormat::Style TopLevelListStyle   = QTextListFormat::ListDisc;
inline constexpr QTextListFormat::Style LowerLevelListStyle = QTextListFormat::ListCircle;
inline constexpr QFont::Weight NormalFontWeight  = QFont::Normal;
inline constexpr QFont::Weight HeadingFontWeight = QFont::DemiBold;
inline constexpr QFont::Weight StrongFontWeight  = QFont::Bold;
#define LINK_COLOR   "#1c37e5"
#define LIST_PADDING "  "

// Optional TextEditor styles (remove to deactivate)
#define DOCUMENT_INDENT_WIDTH  30
#define VIEWPORT_MARGIN        15
#define BLOCK_LINE_HEIGHT      "125%"
#define BLOCK_TOP_MARGIN       0
#define BLOCK_BOTTOM_MARGIN    2
#define HORIZONTAL_RULER_WIDTH "50%"
#define HORIZONTAL_RULER_COLOR "#999"

const QColor &linkColor();
const QString &listPadding();
const int &listPaddingLength();
static const int documentIndentWidth = DOCUMENT_INDENT_WIDTH;
static const int viewportMargin      = VIEWPORT_MARGIN;

// Default block format for TextEditor
const QTextBlockFormat &defaultBlockFormat();
const QTextLength &horizontalRulerWidth();
const QColor &horizontalRulerColor();

// Default char format for TextEditor
// Needs to be initialized after QGuiApplication starts running
const QTextCharFormat &defaultCharFormat();
int defaultFontPointSize();

// Encapsulated formatting logic
bool isMarkdownStrong(const QTextCharFormat &charFormat);
QFont::Weight blockDefaultFontWeight(const QTextBlock &block);

} // namespace TextEditorStyle

#endif
