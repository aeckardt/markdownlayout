#ifndef HTMLSTYLE_H
#define HTMLSTYLE_H

class QByteArray;
template<typename Key, typename T>
class QHash;
class QTextCharFormat;

typedef QHash<QByteArray, QByteArray> HtmlAttributes;
typedef QHash<QByteArray, QByteArray> CssDeclarations;
typedef QHash<QByteArray, CssDeclarations> CssRules;

bool applyCssToCharFormat(const CssDeclarations &style, QTextCharFormat &charFormat);
CssDeclarations parseCss(const QByteArray &styleStr);

#endif
