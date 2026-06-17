#include "texteditor/markdownimporter.h"
#include "texteditor/markdownimporter_p.h"
#include "texteditor/htmlstyle.h"
#include "texteditor/texteditorstyle.h"

#include <QRegularExpression>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFormat>
#include <QTextList>
#include <QTextListFormat>

using namespace Qt::StringLiterals;
using namespace TextEditorStyle;
using TokenType = MarkdownBlockToken::Type;
using NodeType = InlineNode::Type;

QVector<MarkdownBlockToken> MarkdownParser::parse(const QString &markdown)
{
    return MarkdownParser::parseBlocks(markdown);
}

QVector<MarkdownBlockToken> MarkdownParser::parseBlocks(const QString &markdown)
{
    QVector<MarkdownBlockToken> tokens;
    for (const QString &line : markdown.split(QLatin1Char('\n')))
        tokens.append(parseBlockLine(line.endsWith(QLatin1Char('\r')) ? line.left(line.size() - 1) : line));
    return tokens;
}

MarkdownBlockToken MarkdownParser::parseBlockLine(const QString &line)
{
    static const QRegularExpression headingRe("^(#{1,4})\\s+(.*)"_L1);
    static const QRegularExpression unorderedListRe("^[-*]\\s+(.*)"_L1);
    static const QRegularExpression blockQuoteRe(">\\s+(.*)"_L1);

    int indent = 0;
    while (indent < line.size() && line.at(indent).isSpace())
        ++indent;
    const QString stripped = line.mid(indent);

    auto headingMatch = headingRe.match(stripped);
    if (headingMatch.hasMatch())
        return {TokenType::Heading,
                int(headingMatch.captured(1).size()),
                parseInline(headingMatch.captured(2))};

    auto listMatch = unorderedListRe.match(stripped);
    if (listMatch.hasMatch())
        return {TokenType::ListItem,
                int(indent / 2),
                parseInline(listMatch.captured(1))};

    auto blockQuoteMatch = blockQuoteRe.match(stripped);
    if (blockQuoteMatch.hasMatch())
        return {TokenType::BlockQuote,
                0, parseInline(blockQuoteMatch.captured(1))};

    if (stripped == "---"_L1)
        return {TokenType::HorizontalRule, 0, {}};

    if (stripped.isEmpty())
        return {TokenType::Blank, 0, {}};

    return {TokenType::Paragraph, 0, parseInline(line)};
}

QVector<InlineNodePtr> MarkdownParser::parseInline(const QString &text)
{
    return MarkdownInlineParser(text).astRoot()->children;
}

QTextDocument *MarkdownRenderer::createDocument(const QVector<MarkdownBlockToken> &tokens, QObject *parent)
{
    QTextDocument *document = new QTextDocument(parent);
    QTextCursor cursor(document);

    // Do not create an undo when creating the document from the input
    document->setUndoRedoEnabled(false);
    cursor.beginEditBlock();

    // Traverse the syntax tree and render each token / inlinenode
    MarkdownRenderer renderer(&cursor);
    renderer.renderBlocks(tokens);

    // Restore undoRedoEnabled
    cursor.endEditBlock();
    document->setUndoRedoEnabled(true);

    return document;
}

MarkdownRenderer::MarkdownRenderer(QTextCursor *cursor)
    : m_cursor(cursor),
      m_blockFmt(defaultBlockFormat()),
      m_charFmt(defaultCharFormat()),
      m_atBeginning(true),
      m_currentList(nullptr)
{
    Q_ASSERT(m_cursor);
}

void MarkdownRenderer::renderBlocks(const QVector<MarkdownBlockToken> &tokens)
{
    for (const auto &token : tokens) {
        if (!m_atBeginning)
            insertBlock();
        else
            m_atBeginning = false;

        // Apply block / char format changes
        switch (token.type) {
        case TokenType::Heading:
            m_blockFmt.setHeadingLevel(token.level);
            m_charFmt.setFontWeight(HeadingFontWeight);
            m_charFmt.setProperty(QTextFormat::FontSizeAdjustment, 4 - token.level);
            break;
        case TokenType::ListItem:
            if (!m_currentList) {
                if (m_cursor->currentList())
                    m_currentList = m_cursor->currentList();
                else
                    m_currentList = m_cursor->createList(QTextListFormat::ListDisc);
            }

            m_blockFmt.setObjectIndex(m_currentList->objectIndex());
            m_blockFmt.setIndent(token.level);
            m_blockFmt.setProperty(QTextFormat::ListStyle,
                                      token.level > 0 ? LowerLevelListStyle : TopLevelListStyle);
            m_currentList->add(m_cursor->block());

            break;
        case TokenType::BlockQuote:
            m_blockFmt.setProperty(QTextFormat::BlockQuoteLevel, 1);
            break;
        case TokenType::HorizontalRule:
            m_blockFmt.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
            if (horizontalRulerColor().isValid())
                m_blockFmt.setProperty(QTextFormat::BackgroundBrush, horizontalRulerColor());
            break;
        default:
            ;
        }

        // Render children
        if (!token.children.isEmpty())
            renderInlines(token.children);

        // Reset char format changes
        switch (token.type) {
        case TokenType::Heading:
            m_charFmt.setFontWeight(NormalFontWeight);
            m_charFmt.clearProperty(QTextFormat::FontSizeAdjustment);
        default:
            // Nothing to do in other cases
            ;
        }

        finalizeBlock();
    }
}

void MarkdownRenderer::renderInlines(const QVector<InlineNodePtr> &nodes)
{
    for (const auto &node : nodes) {
        if (node->type == NodeType::Text)
            m_cursor->insertText(node->content, m_charFmt);
        // Leaf nodes without text, such as images, are ignored for now.
        else if (!node->children.isEmpty()) {
            const QTextCharFormat oldFmt(m_charFmt);
            applyNodeStyle(node, m_charFmt);
            m_cursor->setCharFormat(m_charFmt);
            renderInlines(node->children);
            m_charFmt = oldFmt;
            m_cursor->setCharFormat(m_charFmt);
        }
    }
}

void MarkdownRenderer::insertBlock()
{
    m_cursor->insertBlock();
    m_blockFmt = defaultBlockFormat();
    m_charFmt = defaultCharFormat();
    m_cursor->setCharFormat(m_charFmt);
}

void MarkdownRenderer::finalizeBlock()
{
    // Set (possibly modified) block format before adding new block
    m_cursor->setBlockFormat(m_blockFmt);

    // It may be necessary to restore QTextCharFormat changes before adding a new block
    // Otherwise the contents on the next line may not be rendered correctly
    m_cursor->setCharFormat(m_charFmt);
}

void MarkdownRenderer::applyNodeStyle(const InlineNodePtr &node, QTextCharFormat &fmt) const
{
    switch (node->type) {
    case NodeType::Strong:
        fmt.setFontWeight(StrongFontWeight);
        break;
    case NodeType::Emph:
        fmt.setFontItalic(true);
        break;
    case NodeType::InlineLink:
        fmt.setAnchor(true);
        fmt.setAnchorHref(node->attrs.value("href"_L1).toString());
        fmt.setForeground(linkColor());
        break;
    case NodeType::HtmlTag:
        if (node->content == "ins"_L1) {
            fmt.setFontUnderline(true);
        } else if (node->content == "span"_L1) {
            const QString style = node->attrs.value("style"_L1).toString();
            if (!style.isEmpty())
                applyHtmlStyle(parseProperties(style), fmt);
        }
        break;
    default:
        break;
    }
};


QTextDocument *documentFromMarkdown(const QString &markdown, QObject *parent)
{
    QVector<MarkdownBlockToken> tokens = MarkdownParser::parse(markdown);
    return MarkdownRenderer::createDocument(tokens, parent);
}
