#ifndef MARKDOWNIMPORTER_H
#define MARKDOWNIMPORTER_H

#include "texteditor/markdowninlineparser.h"

#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextList>
#include <memory>

struct BlockToken
{
    enum class Type {
        Heading,
        ListItem,
        HorizontalRule,
        Paragraph,
        Blank
    };
    Type type = Type::Paragraph;
    int level = 0;
    QVector<InlineNodePtr> children;
};

class MarkdownImporter
{
public:
    explicit MarkdownImporter(QTextDocument *document);

    void import(QString markdown);

private:
    QVector<BlockToken> parseBlocks(const QString &markdown);
    BlockToken parseBlockLine(const QString &line);
    QVector<InlineNodePtr> parseInline(const QString &text);
    void renderBlocks(QTextCursor &cursor, const QVector<BlockToken> &tokens);
    void renderInlines(QTextCursor &cursor, const QVector<InlineNodePtr> &nodes);
    void newLine(QTextCursor &cursor);
    void endLine(QTextCursor &cursor);

    QTextDocument *m_document;

    QString m_input;
    QTextBlockFormat m_blockFormat;
    QTextCharFormat m_charFormat;
    bool m_atBeginning = true;
    QTextList *m_currentList = nullptr;

    using NodeType = InlineNode::Type;
};

#endif
