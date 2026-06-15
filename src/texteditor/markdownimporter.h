#ifndef MARKDOWNIMPORTER_H
#define MARKDOWNIMPORTER_H

#include "texteditor/markdowninlineparser.h"

#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextList>
#include <memory>

struct MarkdownBlockToken
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

class MarkdownParser
{
public:
    static QVector<MarkdownBlockToken> parse(const QString &markdown);

private:
    MarkdownParser() {}

    QVector<MarkdownBlockToken> parseBlocks(const QString &markdown) const;
    MarkdownBlockToken parseBlockLine(const QString &line) const;
    QVector<InlineNodePtr> parseInline(const QString &text) const;
};

class MarkdownRenderer
{
public:
    static QTextDocument *createDocument(const QVector<MarkdownBlockToken> &tokens, QObject *parent = nullptr);

private:
    explicit MarkdownRenderer(QTextCursor *cursor);

    // Render state variables
    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFormat;
    QTextCharFormat m_charFormat;
    bool m_atBeginning = true;
    QTextList *m_currentList = nullptr;

    // Render functions
    void renderBlocks(const QVector<MarkdownBlockToken> &tokens);
    void renderInlines(const QVector<InlineNodePtr> &nodes);
    void insertBlock();
    void finalizeBlock();

    void applyNodeStyle(InlineNodePtr node, QTextCharFormat &fmt) const;

    using TokenType = MarkdownBlockToken::Type;
    using NodeType = InlineNode::Type;
};

class MarkdownImporter
{
public:
    static QTextDocument *createDocument(const QString &markdown, QObject *parent = nullptr);

private:
    explicit MarkdownImporter() {}
};

#endif
