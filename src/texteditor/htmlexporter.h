#ifndef HTMLEXPORTER_H
#define HTMLEXPORTER_H

#include <QHash>
#include <QTextCursor>
#include <QTextDocument>
#include <QString>
#include <QVector>

#include "htmlstyle.h"

class HtmlExporter {
public:
    static QString exportDocument(QTextDocument *document, const QTextCursor *range = nullptr, bool skipHeader = false);

private:
    HtmlExporter(QTextDocument *document, const QTextCursor *range, bool skipHeader);

    QString exportAll();
    QString exportBlock(const QTextBlock &block);
    QString blockFormatToHtml(const QTextBlock &block, bool open);
    QString inlineFormatToHtml(const struct ExportableFragment &fragment, bool open) const;

    static QString htmlHeader();
    static QString htmlFooter();

    struct Tag {
        QString name;
        CssProperties attrs;

        inline Tag(const QString &name, const CssProperties &attrs = {})
            : name(name), attrs(attrs)
        {}
    };

    QTextDocument *m_document;
    int m_start;
    int m_end;
    bool m_skipHeader;
    QVector<Tag> m_openTags;
};

#endif
