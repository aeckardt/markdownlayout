#ifndef MARKDOWNWRITER_H
#define MARKDOWNWRITER_H

class QString;
class QTextCursor;
class QTextDocument;

QString markdownFromDocument(QTextDocument *document, const QTextCursor *range = nullptr);

#endif
