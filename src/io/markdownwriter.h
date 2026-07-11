#ifndef MARKDOWNWRITER_H
#define MARKDOWNWRITER_H

class QByteArray;
class QTextCursor;
class QTextDocument;

QByteArray markdownFromDocument(QTextDocument *document, const QTextCursor *range = nullptr);

#endif
