#ifndef HTMLSTYLE_H
#define HTMLSTYLE_H

#include <QHash>
#include <QString>
#include <QTextCharFormat>

typedef QHash<QString, QString> CssProperties;
typedef QHash<QString, CssProperties> CssRules;

bool applyHtmlStyle(const CssProperties &style, QTextCharFormat &charFormat);
CssProperties parseProperties(const QString &propertiesStr);

#endif
