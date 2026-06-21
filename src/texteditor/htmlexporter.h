#ifndef HTMLEXPORTER_H
#define HTMLEXPORTER_H

class QString;
class QTextDocument;
class QTextCursor;

QString htmlFromDocument(QTextDocument *document, const QTextCursor *range = nullptr, bool skipHeader = false);

#endif
