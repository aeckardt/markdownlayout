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

using namespace TextEditorStyle;

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
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,4})\\s+(.*)"));
    static const QRegularExpression unorderedListRe(QStringLiteral("^[-*]\\s+(.*)"));

    int indent = 0;
    while (indent < line.size() && line.at(indent).isSpace())
        ++indent;
    const QString stripped = line.mid(indent);

    auto headingMatch = headingRe.match(stripped);
    if (headingMatch.hasMatch()) {
        return {MarkdownBlockToken::Type::Heading,
                int(headingMatch.captured(1).size()),
                parseInline(headingMatch.captured(2))};
    }

    auto listMatch = unorderedListRe.match(stripped);
    if (listMatch.hasMatch()) {
        return {MarkdownBlockToken::Type::ListItem,
                int(indent / 2),
                parseInline(listMatch.captured(1))};
    }

    if (stripped == QStringLiteral("---"))
        return {MarkdownBlockToken::Type::HorizontalRule, 0, {}};

    if (stripped.isEmpty())
        return {MarkdownBlockToken::Type::Blank, 0, {}};

    return {MarkdownBlockToken::Type::Paragraph, 0, parseInline(line)};
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
      m_blockFormat(defaultBlockFormat()),
      m_charFormat(defaultCharFormat()),
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

        if (token.type == TokenType::ListItem) {
            m_cursor->insertText(listPadding(), defaultCharFormat());

            if (!m_currentList) {
                if (m_cursor->currentList())
                    m_currentList = m_cursor->currentList();
                else
                    m_currentList = m_cursor->createList(QTextListFormat::ListDisc);
            }

            m_blockFormat.setObjectIndex(m_currentList->objectIndex());
            m_blockFormat.setIndent(token.level);
            m_blockFormat.setProperty(QTextFormat::ListStyle,
                                      token.level > 0 ? LowerLevelListStyle : TopLevelListStyle);
            m_currentList->add(m_cursor->block());
        } else if (token.type == TokenType::Heading) {
            m_blockFormat.setHeadingLevel(token.level);
            m_charFormat.setFontWeight(HeadingFontWeight);
            m_charFormat.setProperty(QTextFormat::FontSizeAdjustment, 4 - token.level);
        } else if (token.type == TokenType::HorizontalRule) {
            m_blockFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
            if (horizontalRulerColor().isValid())
                m_blockFormat.setProperty(QTextFormat::BackgroundBrush, horizontalRulerColor());
        }

        if (!token.children.isEmpty())
            renderInlines(token.children);

        if (token.type == TokenType::Heading) {
            m_charFormat.setFontWeight(NormalFontWeight);
            m_charFormat.clearProperty(QTextFormat::FontSizeAdjustment);
        }

        finalizeBlock();
    }
}

void MarkdownRenderer::renderInlines(const QVector<InlineNodePtr> &nodes)
{
    for (const auto &node : nodes) {
        if (node->type == NodeType::Text)
            m_cursor->insertText(node->content, m_charFormat);
        // TODO: Handle other types?!
        else if (!node->children.isEmpty()) {
            const QTextCharFormat oldFmt(m_charFormat);
            applyNodeStyle(node, m_charFormat);
            m_cursor->setCharFormat(m_charFormat);
            renderInlines(node->children);
            m_charFormat = oldFmt;
            m_cursor->setCharFormat(m_charFormat);
        }
    }
}

void MarkdownRenderer::insertBlock()
{
    m_cursor->insertBlock();
    m_blockFormat = defaultBlockFormat();
    m_charFormat = defaultCharFormat();
    m_cursor->setCharFormat(m_charFormat);
}

void MarkdownRenderer::finalizeBlock()
{
    m_cursor->setBlockFormat(m_blockFormat);
    m_cursor->setCharFormat(m_charFormat);
}

void MarkdownRenderer::applyNodeStyle(InlineNodePtr node, QTextCharFormat &fmt) const
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
        fmt.setAnchorHref(node->attrs.value(QStringLiteral("href")).toString());
        fmt.setForeground(linkColor());
        break;
    case NodeType::HtmlTag:
        if (node->content == QStringLiteral("ins")) {
            fmt.setFontUnderline(true);
        } else if (node->content == QStringLiteral("span")) {
            const QString style = node->attrs.value(QStringLiteral("style")).toString();
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
