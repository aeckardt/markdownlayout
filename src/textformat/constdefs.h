#ifndef CONSTDEFS_H
#define CONSTDEFS_H

#include <QFont>
#include <QTextListFormat>

class QColor;
class QTextBlock;
class QTextBlockFormat;
class QTextCharFormat;
class QTextLength;

/*
 * Editor style constants:
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
 * keep the #ifdef inside a helper function.
 * - If a value is always available and not optional,
 * use a normal constant or function instead of a macro.
*/

// Modifiable TextEditor style constants
inline constexpr QTextListFormat::Style TopLevelListStyle   = QTextListFormat::ListDisc;
inline constexpr QTextListFormat::Style LowerLevelListStyle = QTextListFormat::ListCircle;

inline constexpr QFont::Weight NormalFontWeight  = QFont::Normal;
inline constexpr QFont::Weight HeadingFontWeight = QFont::DemiBold;
inline constexpr QFont::Weight StrongFontWeight  = QFont::Bold;

// Static const variables available with initialization
const QColor &linkColor();

const QTextLength &horizontalRulerWidth();
const QColor &horizontalRulerColor();

// Default block format for TextEditor
const QTextBlockFormat &defaultBlockFormat();

// Default char format for TextEditor
// Needs to be initialized after QGuiApplication starts running
const QTextCharFormat &defaultCharFormat();
int defaultFontPointSize();

// Encapsulated formatting logic
bool isStrong(const QTextCharFormat &charFormat);
QFont::Weight blockDefaultFontWeight(const QTextBlock &block);

#endif // CONSTDEFS_H
