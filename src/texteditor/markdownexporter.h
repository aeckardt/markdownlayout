#ifndef MARKDOWNEXPORTER_H
#define MARKDOWNEXPORTER_H

class QString;
class QTextCursor;
class QTextDocument;

QString markdownFromDocument(QTextDocument *document, const QTextCursor *range = nullptr);

#endif
