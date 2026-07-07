#ifndef HTMLSTYLE_H
#define HTMLSTYLE_H

#include <QByteArray>
#include <QHash>

class QTextCharFormat;

typedef QHash<QByteArray, QByteArray> CssProperties;
typedef QHash<QByteArray, CssProperties> CssRules;

bool applyHtmlStyle(const CssProperties &style, QTextCharFormat &charFormat);
CssProperties parseProperties(const QByteArray &propertiesStr);

#endif
