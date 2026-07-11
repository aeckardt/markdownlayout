#include "htmlimporter.h"

#include "htmlparser.h"
#include "htmlstyle.h"
#include "textformat/blocktypes.h"
#include "textformat/constdefs.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextList>
#include <QVector>

#include <memory>

using namespace Qt::StringLiterals;
using NodeType = HtmlNode::Type;

class HtmlRenderContext
{
public:
    void parseHeadNode(const HtmlNodePtr &headNode);
    CssProperties getStyleFor(const HtmlNodePtr &node) const;

private:
    typedef QHash<QByteArray, QByteArray> HtmlMetadata;

    // Parse inline style string (e.g., "color: red; font-weight: bold;") into a dictionary.
    static CssProperties parseInlineString(const QByteArray &styleStr);

    // Parse CSS string into m_rules dictionary
    void parseCssRules(const QByteArray &cssText);

    CssRules m_rules;
    HtmlMetadata m_metadata;
};

class HtmlRenderer
{
public:
    static QTextDocument *createDocument(const HtmlNodePtr &bodyNode, const HtmlRenderContext &context,
                                         QObject *parent = nullptr);

private:
    explicit HtmlRenderer(QTextCursor *cursor, const HtmlRenderContext &context);

    // Render functions
    void renderNode(const HtmlNodePtr &node);
    void insertBlock();
    void finalizeBlock();

    // Render context for style compilation
    const HtmlRenderContext &m_context;

    // Render state variables
    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFmt;
    QTextCharFormat m_charFmt;

    bool m_atBeginning;
    bool m_newParagraph;
    bool m_newListItem;

    int m_indent;
    int m_nestedUlTags;
    QTextList *m_currentList;
};

void HtmlRenderContext::parseHeadNode(const HtmlNodePtr &headNode)
{
    const QVector<HtmlNodePtr> &children = headNode->children();
    for (const HtmlNodePtr &child : children) {
        if (child->type() == NodeType::HtmlTag && child->tag()->name() == "style") {
            for (const HtmlNodePtr &styleNode : child->children()) {
                if (styleNode->type() == NodeType::Text) {
                    const QByteArray &styleText = styleNode->text();
                    parseCssRules(styleText);
                }
            }
        } else if (child->type() == NodeType::HtmlTag && child->tag()->name() == "meta") {
            const CssProperties &attrs = child->tag()->attrs();
            if (attrs.contains("name") && attrs.contains("content"))
                m_metadata.insert(attrs.value("name"), attrs.value("content"));
        }
    }
}

CssProperties HtmlRenderContext::getStyleFor(const HtmlNodePtr &node) const
{
    if (node->type() != NodeType::HtmlTag)
        return {};

    CssProperties style;
    if (m_rules.contains(node->tag()->name()))
        style = m_rules.value(node->tag()->name());

    // Merge initial styles
    QByteArray inlineStr = node->tag()->attrs().value("style");
    if (!inlineStr.isEmpty())
        style.insert(parseInlineString(inlineStr));

    return style;
}

CssProperties HtmlRenderContext::parseInlineString(const QByteArray &styleStr)
{
    CssProperties attrs;
    QByteArrayList properties = styleStr.split(';');
    for (QByteArray property : properties) {
        property = property.trimmed();
        if (property.isEmpty() || !property.contains(':'))
            continue;

        // Extract name and value by splitting at ':'
        QByteArrayList parts = property.split(':');
        if (parts.count() != 2)
            // Ignore mal-formed input
            continue;
        QByteArray name = parts.at(0).trimmed();
        QByteArray value = parts.at(1).trimmed();
        attrs.insert(name, value);
    }
    return attrs;
}

/* Parses a simple CSS string and inserts the properties into m_rules dictionary
 * mapping individual selectors to their respective property dictionaries.
 *
 * Example:
 *     Input:
 *         "p { margin-top: 10px; color: red; } h1 { font-size: 20pt; }"
 *     Output:
 *         {
 *             "p": {"margin-top": "10px", "color": "red"},
 *             "h1": {"font-size": "20pt"}
 *         }
 *
 * Handles grouped selectors like:
 *     "p, li { margin-left: 0px; }"
 * correctly as:
 *     {
 *         "p": {"margin-left": "0px"},
 *         "li": {"margin-left": "0px"}
 *    }
 *
 * It currently does not handle every legal edge case, for example semicolons inside quotes strings. */
void HtmlRenderContext::parseCssRules(const QByteArray &cssText)
{
    /*
    static const QRegularExpression rulePattern(R"(([^{]+)\{([^}]+)\})");

    auto it = rulePattern.globalMatch(cssText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();

        const QByteArray selectorBlock = match.captured(1);
        const QByteArray propertiesStr = match.captured(2).trimmed();

        const CssProperties properties = parseProperties(propertiesStr);

        const QByteArrayList selectors =
            selectorBlock.split(QLatin1Char(','), Qt::SkipEmptyParts);

        for (const QByteArray &rawSelector : selectors) {
            const QByteArray selector = rawSelector.trimmed();

            if (selector.isEmpty())
                continue;

            if (m_rules.contains(selector))
                m_rules[selector].insert(properties);
            else
                m_rules.insert(selector, properties);
        }
    }
    */
}

QTextDocument *HtmlRenderer::createDocument(const HtmlNodePtr &bodyNode, const HtmlRenderContext &context,
                                            QObject *parent)
{
    QTextDocument *document = new QTextDocument(parent);
    QTextCursor cursor(document);

    // Do not create an undo when creating the document from the input
    document->setUndoRedoEnabled(false);
    cursor.beginEditBlock();

    // Traverse the syntax tree and render each node
    HtmlRenderer renderer(&cursor, context);
    renderer.renderNode(bodyNode);

    // Restore undoRedoEnabled
    cursor.endEditBlock();
    document->setUndoRedoEnabled(true);

    return document;
}

HtmlRenderer::HtmlRenderer(QTextCursor *cursor, const HtmlRenderContext &context)
    : m_context(context),
      m_cursor(cursor),
      m_blockFmt(defaultBlockFormat()),
      m_charFmt(defaultCharFormat()),
      m_atBeginning(true),
      m_newParagraph(false),
      m_newListItem(false),
      m_indent(0),
      m_nestedUlTags(0),
      m_currentList(nullptr)
{
    Q_ASSERT(m_cursor);
}

void HtmlRenderer::renderNode(const HtmlNodePtr &node)
{
    if (node->type() == NodeType::Text) {
        if (!node->text().isEmpty()) {
            // Insert text with the current char format
            m_cursor->insertText(node->text(), m_charFmt);

            // Remove newline guards because text has been added
            m_newParagraph = false;
            m_newListItem = false;

        }
        return;
    }

    // Indicates if m_charFmt has changed within this function
    bool fmtChanged = false;

    // Handle tags
    const QByteArray tag = node->tag()->name();
    if (tag == "p") {
        // Safety guard for not adding a new line directly after a list item
        if (!m_newListItem)
            insertBlock();
        else
            m_newListItem = false;
        m_newParagraph = true;
    } else if (tag == "br") {
        if (!m_newParagraph) {
            finalizeBlock();
            insertBlock();
        } else
            // Don't add a new line if the paragraph just started
            // Because a new line has already been added!
            m_newParagraph = false;
        // Since br is a self enclosing tag, it's safe to return
        finalizeBlock();
        return;
    } else if (tag == "hr") {
        if (!m_newParagraph) {
            finalizeBlock();
            insertBlock();
        } else
            // # Don't add a new line if the paragraph just started
            // # Because a new line has already been added!
            m_newParagraph = false;
        // Set horizontal rule property for this block
        m_blockFmt.setProperty(QTextFormat::Property::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
        if (horizontalRulerColor().isValid())
            m_blockFmt.setProperty(QTextFormat::Property::BackgroundBrush, horizontalRulerColor());
        // Since hr is a self enclosing tag, it's safe to return
        finalizeBlock();
        return;
    } else if (tag == "strong" || tag == "b") {
        m_charFmt.setFontWeight(StrongFontWeight);
        fmtChanged = true;
    } else if (tag == "em" || tag == "i") {
        m_charFmt.setFontItalic(true);
        fmtChanged = true;
    } else if (tag == "ins" || tag == "u") {
        m_charFmt.setFontUnderline(true);
        fmtChanged = true;
    } else if (tag == "a") {
        m_charFmt.setAnchor(true);
        if (node->tag()->attrs().contains("href"))
            m_charFmt.setAnchorHref(node->tag()->attrs().value("href"));
        m_charFmt.setForeground(linkColor());
        fmtChanged = true;
    } else if (tag == "ul") {
        // Increase value for more indent (in case more than one ul tag is used)
        ++m_nestedUlTags;
    } else if (tag == "li") {
        if (m_nestedUlTags <= 0)
            qDebug("Warning: Misplaced <li> tag. Lists might not be added properly.");

        // Use new line for list item
        insertBlock();

        if (!m_currentList) {
            if (m_cursor->currentList())
                m_currentList = m_cursor->currentList();
            else
                // Setup list, if not yet available
                m_currentList = m_cursor->createList(TopLevelListStyle);
        }

        // Attach QTextList object to block format
        m_blockFmt.setObjectIndex(m_currentList->objectIndex());

        // Store marker style as block-level layout metadata
        if (m_nestedUlTags > 1)
            m_blockFmt.setProperty(QTextFormat::ListStyle, LowerLevelListStyle);
        else
            m_blockFmt.setProperty(QTextFormat::ListStyle, TopLevelListStyle);

        // Activate safety guard for not adding a newline with a paragraph directly after
        m_newListItem = true;
    } else if (tag == "h1" || tag == "h2" || tag == "h3" ||
               tag == "h4" || tag == "h5" || tag == "h6") {
        // Start heading in a new block
        insertBlock();

        // Set heading level property
        const int headingLevel = tag.at(1) - '0';
        m_blockFmt.setHeadingLevel(headingLevel);

        // Adjust char format for new block
        QTextCharFormat headingCharFmt = headingFormatModifier(headingLevel, QTextCharFormat());
        m_cursor->mergeBlockCharFormat(headingCharFmt);

        // Use heading char format for next fragment(s)
        m_charFmt.merge(headingCharFmt);
        fmtChanged = true;
    }

    // Handle styles
    const CssProperties style = m_context.getStyleFor(node);

    // Apply styles to m_charFmt
    fmtChanged = fmtChanged || applyCssToCharFormat(style, m_charFmt);

    if (m_newParagraph && tag != "p")
        // Remove guard for not adding a new line when a <br /> tag follows directly after a paragraph
        m_newParagraph = false;
    if (m_newListItem && tag != "li")
        // Remove guard for not adding a new line when a <p> tag follows directly after a list item
        m_newListItem = false;

    // Change char format for the following bracket
    QTextCharFormat oldFmt(m_cursor->charFormat());
    if (fmtChanged)
        m_cursor->setCharFormat(m_charFmt);

    // Iterate over children
    for (const HtmlNodePtr &child : node->children())
        renderNode(child);

    // Restore previous char format
    if (fmtChanged) {
        m_charFmt = oldFmt;
        m_cursor->setCharFormat(m_charFmt);
    }

    // Handle closing tags
    if (tag == "p") {
        if (!m_newListItem)
            finalizeBlock();
        m_newParagraph = false;
    } else if (tag == "ul") {
        --m_nestedUlTags;
        if (m_nestedUlTags == 0) {
            // Remove list reference
            m_currentList = nullptr;
            m_blockFmt.setObjectIndex(-1);
        }
    } else if (tag == "li")
        finalizeBlock();
    else if (tag == "h1" || tag == "h2" || tag == "h3" ||
             tag == "h4" || tag == "h5" || tag == "h6") {
        finalizeBlock();
        m_blockFmt.setHeadingLevel(0);
    }
}

void HtmlRenderer::insertBlock()
{
    // The first insertBlock called shouldn't add another block
    // Therefore this guard is necessary
    if (m_atBeginning) {
        m_atBeginning = false;
        return;
    }

    m_cursor->insertBlock();

    // Reset block format
    m_indent = 0;
    m_blockFmt = QTextBlockFormat(defaultBlockFormat());

    // Reset char format (with default font size)
    m_charFmt = QTextCharFormat(defaultCharFormat());
    m_cursor->setCharFormat(m_charFmt);
    m_cursor->setBlockCharFormat(m_charFmt);
}

void HtmlRenderer::finalizeBlock()
{
    // Set block format with indent, heading level / blockquote properties
    // before adding new block
    int listIndent = std::max<int>(0, m_nestedUlTags - 1);
    m_blockFmt.setIndent(m_indent + listIndent);
    m_cursor->setBlockFormat(m_blockFmt);
}

QTextDocument *documentFromHtml(const QByteArray &html, QObject *parent)
{
    // Build syntax tree
    HtmlParser parser(html);
    HtmlNodePtr ast = parser.parse();

    // Find <html> tag
    HtmlNodePtr htmlNode = parser.findNode(ast, "html");
    if (!htmlNode)
        // Put everything inside <html>...</html> tag
        htmlNode = HtmlNode::createHtmlTag(
                    HtmlTag::create("html", HtmlTag::OpenTag),
                    std::move(ast->children()));

    // Setup context for tag styles
    HtmlRenderContext context;
    HtmlNodePtr headNode = parser.findNode(htmlNode, "head");
    if (headNode)
        context.parseHeadNode(headNode);

    // Find <body> tag
    HtmlNodePtr bodyNode = parser.findNode(htmlNode, "body");
    if (!bodyNode)
        // Create bodyNode as root for parsing
        bodyNode = HtmlNode::createHtmlTag(
                    HtmlTag::create("body", HtmlTag::OpenTag),
                    std::move(htmlNode->children()));

    // Render AST onto QTextDocument
    return HtmlRenderer::createDocument(bodyNode, context, parent);
}
