#ifndef MARKDOWNIMPORTER_P_H
#define MARKDOWNIMPORTER_P_H

#include "texteditor/markdowninlineparser.h"

#include <QList>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <memory>

class QTextCursor;
class QTextDocument;
class QTextList;

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
    QList<InlineNodePtr> children;
};

class MarkdownParser
{
public:
    MarkdownParser() = delete;

    static QList<MarkdownBlockToken> parse(const QString &markdown);

private:
    static QList<MarkdownBlockToken> parseBlocks(const QString &markdown);
    static MarkdownBlockToken parseBlockLine(const QString &line);
    static QList<InlineNodePtr> parseInline(const QString &text);
};

class MarkdownRenderer
{
public:
    static QTextDocument *createDocument(const QList<MarkdownBlockToken> &tokens, QObject *parent = nullptr);

private:
    explicit MarkdownRenderer(QTextCursor *cursor);

    // Render state variables
    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFormat;
    QTextCharFormat m_charFormat;
    bool m_atBeginning = true;
    QTextList *m_currentList = nullptr;

    // Render functions
    void renderBlocks(const QList<MarkdownBlockToken> &tokens);
    void renderInlines(const QList<InlineNodePtr> &nodes);
    void insertBlock();
    void finalizeBlock();

    void applyNodeStyle(InlineNodePtr node, QTextCharFormat &fmt) const;

    using TokenType = MarkdownBlockToken::Type;
    using NodeType = InlineNode::Type;
};

#endif // MARKDOWNIMPORTER_P_H
