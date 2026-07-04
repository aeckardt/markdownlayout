#ifndef MARKDOWNIMPORTER_H
#define MARKDOWNIMPORTER_H

class QByteArray;
class QObject;
class QTextDocument;

QTextDocument *documentFromMarkdown(const QByteArray &markdown, QObject *parent = nullptr);

#endif
