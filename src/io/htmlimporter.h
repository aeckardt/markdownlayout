#ifndef HTMLIMPORTER_H
#define HTMLIMPORTER_H

class QObject;
class QString;
class QTextDocument;

/* Imports an HTML string to a QTextDocument from a focused subset of HTML.
 * The main purpose of this importer is to losslessly cut, copy and paste
 * between QTextDocuments created with TextEditor.
 * Therefore it doesn't parse a lot of tags and styles.
 * Should be extended whenever suitable. */
QTextDocument *documentFromHtml(const QString &html, QObject *parent = nullptr);

#endif
