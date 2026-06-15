#ifndef MARKDOWNIMPORTER_H
#define MARKDOWNIMPORTER_H

class QObject;
class QString;
class QTextDocument;

QTextDocument *documentFromMarkdown(const QString &markdown, QObject *parent = nullptr);

#endif
