#include "texteditor/markdownimporter.h"
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

MarkdownImporter::MarkdownImporter(QTextDocument *document)
    : m_document(document)
{
}

void MarkdownImporter::import(QString markdown)
{
    m_input = std::move(markdown);

    QVector<BlockToken> tokens = parseBlocks(m_input);
    m_document->clear();
    QTextCursor cursor(m_document);

    m_document->setUndoRedoEnabled(false);
    cursor.beginEditBlock();

    m_blockFormat = defaultBlockFormat();
    m_charFormat = defaultCharFormat();
    m_atBeginning = true;
    m_currentList = nullptr;

    renderBlocks(cursor, tokens);

    cursor.endEditBlock();
    m_document->setUndoRedoEnabled(true);
}

void MarkdownImporter::renderBlocks(QTextCursor &cursor, const QVector<BlockToken> &tokens)
{
    for (const auto &token : tokens) {
        if (!m_atBeginning)
            newLine(cursor);
        else
            m_atBeginning = false;

        if (token.type == BlockToken::Type::ListItem) {
            cursor.insertText(listPadding(), defaultCharFormat());

            if (!m_currentList) {
                if (cursor.currentList())
                    m_currentList = cursor.currentList();
                else
                    m_currentList = cursor.createList(QTextListFormat::ListDisc);
            }

            m_blockFormat.setObjectIndex(m_currentList->objectIndex());
            m_blockFormat.setIndent(token.level);
            m_blockFormat.setProperty(QTextFormat::ListStyle,
                                      token.level > 0 ? LowerLevelListStyle : TopLevelListStyle);
            m_currentList->add(cursor.block());
        } else if (token.type == BlockToken::Type::Heading) {
            m_blockFormat.setHeadingLevel(token.level);
            m_charFormat.setFontWeight(HeadingFontWeight);
            m_charFormat.setProperty(QTextFormat::FontSizeAdjustment, 4 - token.level);
        } else if (token.type == BlockToken::Type::HorizontalRule) {
            m_blockFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
            if (horizontalRulerColor().isValid())
                m_blockFormat.setProperty(QTextFormat::BackgroundBrush, horizontalRulerColor());
        }

        if (!token.children.isEmpty())
            renderInlines(cursor, token.children);

        if (token.type == BlockToken::Type::Heading) {
            m_charFormat.setFontWeight(NormalFontWeight);
            m_charFormat.clearProperty(QTextFormat::FontSizeAdjustment);
        }

        endLine(cursor);
    }
}

void MarkdownImporter::renderInlines(QTextCursor &cursor, const QVector<InlineNodePtr> &nodes)
{
    auto applyNodeStyle = [&](InlineNodePtr node, QTextCharFormat &fmt) {
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

    for (const auto &node : nodes) {
        if (node->type == InlineNode::Type::Text) {
            cursor.insertText(node->content, m_charFormat);
        } else if (!node->children.isEmpty()) {
            const QTextCharFormat old = m_charFormat;
            applyNodeStyle(node, m_charFormat);
            cursor.setCharFormat(m_charFormat);
            renderInlines(cursor, node->children);
            m_charFormat = old;
            cursor.setCharFormat(m_charFormat);
        }
    }
}

void MarkdownImporter::newLine(QTextCursor &cursor)
{
    cursor.insertBlock();
    m_blockFormat = defaultBlockFormat();
    m_charFormat = defaultCharFormat();
    cursor.setCharFormat(m_charFormat);
}

void MarkdownImporter::endLine(QTextCursor &cursor)
{
    cursor.setBlockFormat(m_blockFormat);
    cursor.setCharFormat(m_charFormat);
}

QVector<BlockToken> MarkdownImporter::parseBlocks(const QString &markdown)
{
    QVector<BlockToken> tokens;
    for (const QString &line : markdown.split(QLatin1Char('\n')))
        tokens.append(parseBlockLine(line.endsWith(QLatin1Char('\r')) ? line.left(line.size() - 1) : line));
    return tokens;
}

BlockToken MarkdownImporter::parseBlockLine(const QString &line)
{
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,4})\\s+(.*)"));
    static const QRegularExpression unorderedListRe(QStringLiteral("^[-*]\\s+(.*)"));

    int indent = 0;
    while (indent < line.size() && line.at(indent).isSpace())
        ++indent;
    const QString stripped = line.mid(indent);

    auto headingMatch = headingRe.match(stripped);
    if (headingMatch.hasMatch()) {
        return {BlockToken::Type::Heading,
                int(headingMatch.captured(1).size()),
                parseInline(headingMatch.captured(2))};
    }

    auto listMatch = unorderedListRe.match(stripped);
    if (listMatch.hasMatch()) {
        return {BlockToken::Type::ListItem,
                int(indent / 2),
                parseInline(listMatch.captured(1))};
    }

    if (stripped == QStringLiteral("---"))
        return {BlockToken::Type::HorizontalRule, 0, {}};

    if (stripped.isEmpty())
        return {BlockToken::Type::Blank, 0, {}};

    return {BlockToken::Type::Paragraph, 0, parseInline(line)};
}

QVector<InlineNodePtr> MarkdownImporter::parseInline(const QString &text)
{
    return MarkdownInlineParser(text).astRoot()->children;
}
