#ifndef MARKDOWNEXPORTER_H
#define MARKDOWNEXPORTER_H

#include <QTextCursor>
#include <QTextDocument>
#include <QString>

class MarkdownExporter {
public:
    static QString exportDocument(QTextDocument *document, const QTextCursor *range = nullptr);

private:
    MarkdownExporter(QTextDocument *document, const QTextCursor *range);

    QString exportAll();
    QString exportBlock(const QTextBlock &block);
    QPair<QString, int> exportFragment(const struct ExportableFragment &fragment,
                                       bool removeTwoSpaces,
                                       int headingLevel = 0);

    QTextDocument *m_document = nullptr;
    int m_start = 0;
    int m_end = -1;
};

#endif
