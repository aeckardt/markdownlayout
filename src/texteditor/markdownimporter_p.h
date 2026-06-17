#ifndef MARKDOWNIMPORTER_P_H
#define MARKDOWNIMPORTER_P_H

#include "texteditor/markdowninlineparser.h"

#include <QVector>
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
        BlockQuote,
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
    MarkdownParser() = delete;

    static QVector<MarkdownBlockToken> parse(const QString &markdown);

private:
    static QVector<MarkdownBlockToken> parseBlocks(const QString &markdown);
    static MarkdownBlockToken parseBlockLine(const QString &line);
    static QVector<InlineNodePtr> parseInline(const QString &text);
};

class MarkdownRenderer
{
public:
    static QTextDocument *createDocument(const QVector<MarkdownBlockToken> &tokens, QObject *parent = nullptr);

private:
    explicit MarkdownRenderer(QTextCursor *cursor);

    // Render functions
    void renderBlocks(const QVector<MarkdownBlockToken> &tokens);
    void renderInlines(const QVector<InlineNodePtr> &nodes);
    void insertBlock();
    void finalizeBlock();

    void applyNodeStyle(const InlineNodePtr &node, QTextCharFormat &fmt) const;

    // Render state variables
    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFmt;
    QTextCharFormat m_charFmt;
    bool m_atBeginning = true;
    QTextList *m_currentList = nullptr;
};

#endif // MARKDOWNIMPORTER_P_H
