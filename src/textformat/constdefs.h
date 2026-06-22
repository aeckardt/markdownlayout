#ifndef CONSTDEFS_H
#define CONSTDEFS_H

#include <QFont>
#include <QTextListFormat>

class QColor;
class QTextBlock;
class QTextBlockFormat;
class QTextCharFormat;
class QTextLength;

// Modifiable editor style constants
inline constexpr QTextListFormat::Style TopLevelListStyle   = QTextListFormat::ListDisc;
inline constexpr QTextListFormat::Style LowerLevelListStyle = QTextListFormat::ListCircle;

inline constexpr QFont::Weight NormalFontWeight  = QFont::Normal;
inline constexpr QFont::Weight HeadingFontWeight = QFont::DemiBold;
inline constexpr QFont::Weight StrongFontWeight  = QFont::Bold;

// Static const variables available with initialization
const QColor &linkColor();

const QTextLength &horizontalRulerWidth();
const QColor &horizontalRulerColor();

// Default block format for editor
const QTextBlockFormat &defaultBlockFormat();

// Default char format for editor
// Needs to be initialized after QGuiApplication starts running
const QTextCharFormat &defaultCharFormat();
int defaultFontPointSize();

// Derived formatting logic
bool isStrong(const QTextCharFormat &charFormat);
QFont::Weight blockDefaultFontWeight(const QTextBlock &block);

#endif // CONSTDEFS_H
