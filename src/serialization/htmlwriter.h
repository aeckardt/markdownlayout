#ifndef HTMLWRITER_H
#define HTMLWRITER_H

class QString;
class QTextDocument;
class QTextCursor;

QString htmlFromDocument(QTextDocument *document, const QTextCursor *range = nullptr, bool skipHeader = false);

#endif
