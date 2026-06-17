#ifndef TEXTEDITORSTYLE_H
#define TEXTEDITORSTYLE_H

#include <QFont>
#include <QTextListFormat>

class QColor;
class QTextBlockFormat;
class QTextLength;
class QTextCharFormat;
class QTextBlock;

/*
 * Style configuration:
 *
 * Some style values are always part of the editor model and are exposed as
 * normal constants or functions.
 *
 * Optional style values are controlled through preprocessor macros in
 * texteditorstyle_config.h. Removing such a macro means that the corresponding
 * formatting step is not compiled at all.
 *
 * Rule of thumb:
 * - If a style sets a concrete Qt property at exactly one call site,
 * use #ifdef directly at that call site.
 * - If a style is translated into a reusable Qt format object,
 * keep the #ifdef inside TextEditorStyle's helper function.
 * - If a value is always available and not optional,
 * use a normal constant or function instead of a macro.
*/

namespace TextEditorStyle {

// Modifiable TextEditor style constants
inline constexpr QTextListFormat::Style TopLevelListStyle   = QTextListFormat::ListDisc;
inline constexpr QTextListFormat::Style LowerLevelListStyle = QTextListFormat::ListCircle;

inline constexpr QFont::Weight NormalFontWeight  = QFont::Normal;
inline constexpr QFont::Weight HeadingFontWeight = QFont::DemiBold;
inline constexpr QFont::Weight StrongFontWeight  = QFont::Bold;

// Static const variables available with initialization
const QColor &linkColor();

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
