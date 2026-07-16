#ifndef HTMLWRITER_H
#define HTMLWRITER_H

class QByteArray;
class QTextDocument;
class QTextCursor;

QByteArray htmlFromDocument(QTextDocument *document, const QTextCursor *range = nullptr, bool skipHeader = false);

#endif
