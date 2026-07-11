#include "htmlimporter.h"

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

struct HtmlToken {
    enum class Type {
        StartTag,
        EndTag,
        Text,
        SelfClosingTag
    };

    Type type;
    QByteArray name;  // tag name
    CssProperties attrs;
    QByteArray content;  // for text or comments
};

using TokenType = HtmlToken::Type;

struct HtmlNode;
typedef std::shared_ptr<HtmlNode> HtmlNodePtr;

struct HtmlNode {
    enum class Type {
        Element,
        Text
    };

    HtmlNode(const QByteArray &name, const CssProperties &attrs, const QVector<HtmlNodePtr> &children = {})
        : type(Type::Element), name(name), attrs(attrs), children(children) {}
    HtmlNode(const QByteArray &content) : type(Type::Text), content(content) {}

    static HtmlNodePtr makeElement(const QByteArray &name, const CssProperties &attrs,
        const QVector<HtmlNodePtr> &children = {})
    { return std::make_shared<HtmlNode>(name, attrs, children); }

    static HtmlNodePtr makeText(const QByteArray &content)
    { return std::make_shared<HtmlNode>(content); }

    Type type;
    QByteArray name;  // tag name if element
    CssProperties attrs;
    QVector<HtmlNodePtr> children;
    QByteArray content;  // only for text nodes
};

using NodeType = HtmlNode::Type;

class HtmlParser__
{
public:
    // Parse the HTML string to generate an array of roots
    // May be one root if everything is encapsulated in <html> tag
    static QVector<HtmlNodePtr> parse(const QByteArray &html);

    // Recursively searches the AST for the first node with the given tag name.
    static HtmlNodePtr findNode(const HtmlNodePtr &root, const QByteArray &tagName);

    // Search in a list rather than a single root element
    static HtmlNodePtr findNode(const QVector<HtmlNodePtr> &nodes, const QByteArray &tagName);

private:
    explicit HtmlParser__(const QByteArray &html) : m_input(html) {}

    // Tokenizer
    QVector<HtmlToken> tokenize() const;

    // Parser
    QVector<HtmlNodePtr> parseChildNodes(const QVector<HtmlToken> &tokens, int &pos, const QByteArray &stopTag = {}) const;

    const QByteArrayView m_input;
};

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

static void appendUtf8CodePoint(QByteArray &out, uint codePoint)
{
    if (codePoint == 0 ||
        codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        out.append("\xEF\xBF\xBD", 3); // U+FFFD replacement character in UTF-8
        return;
    }

    if (codePoint <= 0x7F) {
        out.append(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        out.append(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.append(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        out.append(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.append(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.append(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.append(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.append(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.append(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.append(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

static QByteArray htmlUnescape(const QByteArray &text)
{
    static const QHash<QByteArray, QByteArray> namedEntities = {
        {"amp"_L1,  "&"_L1},
        {"lt"_L1,   "<"_L1},
        {"gt"_L1,   ">"_L1},
        {"quot"_L1, "\""_L1},
        {"apos"_L1, "'"_L1},
        {"nbsp"_L1, QByteArray(QChar(0x00A0))},

        // Optional common extras:
        {"ndash"_L1, QByteArray(QChar(0x2013))},
        {"mdash"_L1, QByteArray(QChar(0x2014))},
        {"hellip"_L1, QByteArray(QChar(0x2026))}
    };

    QByteArray out;
    out.reserve(text.size());

    qsizetype i = 0;
    while (i < text.size()) {
        if (text.at(i) != QLatin1Char('&')) {
            out.append(text.at(i));
            ++i;
            continue;
        }

        const qsizetype semicolon = text.indexOf(QLatin1Char(';'), i + 1);

        if (semicolon < 0) {
            out.append(text.at(i));
            ++i;
            continue;
        }

        const QByteArray entity = text.mid(i + 1, semicolon - i - 1);

        if (entity.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            uint codePoint = 0;

            QByteArrayView entityView(entity);

            if (entity.startsWith("#x"_L1, Qt::CaseInsensitive))
                codePoint = entityView.sliced(2).toUInt(&ok, 16);
            else
                codePoint = entityView.sliced(1).toUInt(&ok, 10);

            if (ok) {
                appendCodePoint(out, codePoint);
                i = semicolon + 1;
                continue;
            }
        } else {
            const auto it = namedEntities.constFind(entity);
            if (it != namedEntities.constEnd()) {
                out.append(*it);
                i = semicolon + 1;
                continue;
            }
        }

        // Unknown or malformed entity: keep it unchanged.
        out.append(QByteArrayView(text).sliced(i, semicolon - i + 1));
        i = semicolon + 1;
    }

    return out;
}

QVector<HtmlNodePtr> HtmlParser__::parse(const QByteArray &html)
{
    HtmlParser__ parser(html);

    // Parse input into tokens
    QVector<HtmlToken> tokens = parser.tokenize();

    // Initialize current token position
    int pos = 0;

    // Build and return syntax tree
    // It might not be a tree in a strict sense
    // but rather an array of roots
    return parser.parseChildNodes(tokens, pos);
}

/* findNode recursively searches for a node with the given tag name in the AST.
 *
 * Parameters:
 *     root (HtmlNodePtr): The root of the AST or a list of nodes.
 *     tagName (QByteArray): The tag name to search for (case-insensitive).
 *
 * Returns:
 *     HtmlNodePtr: The first node matching the tag name, or nullptr if not found. */
HtmlNodePtr HtmlParser__::findNode(const HtmlNodePtr &root, const QByteArray &tagName)
{
    // Check if the current node matches the tag (case-insensitive).
    if (!root->name.isEmpty() && root->name.toLower() == tagName.toLower())
        return root;

    // Otherwise, search recursively in the children.
    for (const HtmlNodePtr &child : root->children) {
        HtmlNodePtr result = findNode(child, tagName);
        if (result)
            return result;
    }

    return {};
}

HtmlNodePtr HtmlParser__::findNode(const QVector<HtmlNodePtr> &nodes, const QByteArray &tagName)
{
    for (const HtmlNodePtr &node : nodes) {
        HtmlNodePtr result = findNode(node, tagName);
        if (result)
            return result;
    }

    return {};
}

QVector<HtmlToken> HtmlParser__::tokenize() const
{
    static const QRegularExpression tagPattern(R"((?<tag><[^>]+>)|(?<text>[^<]+))");
    static const QRegularExpression attrPattern(
        R"regex((?<name>[a-zA-Z_:][-a-zA-Z0-9_:.]*)\s*=\s*(?<quote>["'])(?<value>.*?)\k<quote>)regex"
    );
    static const QSet<QByteArray> selfClosingTags = {"br", "img", "hr", "meta"};

    QVector<HtmlToken> tokens;

    auto it = tagPattern.globalMatch(m_input);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        const QByteArray tag = match.captured("tag").toUtf8();
        const QByteArray text = match.captured("text").toUtf8();

        if (!text.isEmpty()) {
            if (QByteArray(text).remove('\n').trimmed().isEmpty())
                continue;

            HtmlToken token;
            token.type = TokenType::Text;
            token.content = htmlUnescape(text).toUtf8();

            tokens.append(token);
        }
        else if (!tag.isEmpty()) {
            if (tag.startsWith("</")) {
                // End tag
                const QByteArray tagName = tag.mid(2, tag.length() - 3).trimmed().toLower();

                HtmlToken token;
                token.type = TokenType::EndTag;
                token.name = tagName;

                tokens.append(token);
            }
            else {
                const bool isSelfClosing = tag.endsWith("/>");

                QByteArray tagContent;
                if (isSelfClosing)
                    tagContent = tag.mid(1, tag.length() - 3).trimmed();
                else
                    tagContent = tag.mid(1, tag.length() - 2).trimmed();

                QByteArray tagName;
                QByteArray attrString;

                static const QRegularExpression whitespacePattern(R"(\s)");
                const int firstSpace = tagContent.indexOf(whitespacePattern);

                if (firstSpace < 0)
                    tagName = tagContent.toLower();
                else {
                    tagName = tagContent.left(firstSpace).toLower();
                    attrString = tagContent.mid(firstSpace + 1).trimmed();
                }

                CssProperties attrs;

                auto attrIt = attrPattern.globalMatch(attrString);
                while (attrIt.hasNext()) {
                    QRegularExpressionMatch attrMatch = attrIt.next();

                    const QByteArray name = attrMatch.captured("name").toLower().toUtf8();
                    const QByteArray value = attrMatch.captured("value").toUtf8();

                    attrs.insert(name, value);
                }

                HtmlToken token;
                token.type =
                    isSelfClosing || selfClosingTags.contains(tagName)
                    ? TokenType::SelfClosingTag
                    : TokenType::StartTag;
                token.name = tagName;

                if (!attrs.isEmpty())
                    token.attrs = attrs;

                tokens.append(token);
            }
        }
    }

    return tokens;
}


QVector<HtmlNodePtr> HtmlParser__::parseChildNodes(const QVector<HtmlToken> &tokens, int &pos, const QByteArray &tagName) const
{
    QVector<HtmlNodePtr> nodes;

    bool tagClosed = false;
    while (pos < tokens.size() && !tagClosed) {
        const HtmlToken &token = tokens.at(pos);

        switch (token.type) {
        case TokenType::Text: {
            nodes.append(HtmlNode::makeText(token.content));
            ++pos;
            break;
        }
        case TokenType::StartTag: {
            ++pos;
            QVector<HtmlNodePtr> children = parseChildNodes(tokens, pos, token.name);
            nodes.append(HtmlNode::makeElement(token.name, token.attrs, children));
            break;
        }
        case TokenType::SelfClosingTag: {
            nodes.append(HtmlNode::makeElement(token.name, token.attrs));
            ++pos;
            break;
        }
        case TokenType::EndTag: {
            if (!tagName.isEmpty() && token.name == tagName) {
                tagClosed = true;
                ++pos;
                break;
            } else
                // Unexpected closing tag -> ignore or warn
                ++pos;
            break;
        }
        default:
            ;
        }
    }

    return nodes;
}

void HtmlRenderContext::parseHeadNode(const HtmlNodePtr &headNode)
{
    for (const HtmlNodePtr &child : headNode->children) {
        if (child->name == "style") {
            for (const HtmlNodePtr &styleNode : child->children) {
                const QByteArray &styleText = styleNode->content;
                parseCssRules(styleText);
            }
        } else if (child->name == "meta"_L1) {
            const CssProperties &attrs = child->attrs;
            if (attrs.contains("name") && attrs.contains("content"))
                m_metadata.insert(attrs.value("name"), attrs.value("content"));
        }
    }
}

CssProperties HtmlRenderContext::getStyleFor(const HtmlNodePtr &node) const
{
    CssProperties style;
    if (m_rules.contains(node->name))
        style = m_rules.value(node->name);

    // Merge initial styles
    QByteArray inlineStr = node->attrs.value("style");
    if (!inlineStr.isEmpty())
        style.insert(parseInlineString(inlineStr));

    return style;
}

CssProperties HtmlRenderContext::parseInlineString(const QByteArray &styleStr)
{
    CssProperties attrs;
    QByteArrayList properties = styleStr.split(';', Qt::SkipEmptyParts);
    for (QByteArray property : properties) {
        property = property.trimmed();
        if (property.isEmpty() || !property.contains(':'))
            continue;

        // Extract name and value by splitting at ':'
        QByteArrayList parts = property.split(':', Qt::SkipEmptyParts);
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
    if (node->type == NodeType::Text) {
        if (!node->content.isEmpty()) {
            // Insert text with the current char format
            m_cursor->insertText(node->content, m_charFmt);

            // Remove newline guards because text has been added
            m_newParagraph = false;
            m_newListItem = false;

        }
        return;
    }

    // Indicates if m_charFmt has changed within this function
    bool fmtChanged = false;

    // Handle tags
    const QByteArray tag = node->name.toLower();
    if (tag == "p"_L1) {
        // Safety guard for not adding a new line directly after a list item
        if (!m_newListItem)
            insertBlock();
        else
            m_newListItem = false;
        m_newParagraph = true;
    } else if (tag == "br"_L1) {
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
    } else if (tag == "hr"_L1) {
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
    } else if (tag == "strong"_L1 || tag == "b"_L1) {
        m_charFmt.setFontWeight(StrongFontWeight);
        fmtChanged = true;
    } else if (tag == "em"_L1 || tag == "i"_L1) {
        m_charFmt.setFontItalic(true);
        fmtChanged = true;
    } else if (tag == "ins"_L1 || tag == "u"_L1) {
        m_charFmt.setFontUnderline(true);
        fmtChanged = true;
    } else if (tag == "a"_L1) {
        m_charFmt.setAnchor(true);
        if (node->attrs.contains("href"_L1))
            m_charFmt.setAnchorHref(node->attrs.value("href"_L1));
        m_charFmt.setForeground(linkColor());
        fmtChanged = true;
    } else if (tag == "ul"_L1) {
        // Increase value for more indent (in case more than one ul tag is used)
        ++m_nestedUlTags;
    } else if (tag == "li"_L1) {
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
        const int headingLevel = tag.at(1).digitValue();
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

    if (m_newParagraph && tag != "p"_L1)
        // Remove guard for not adding a new line when a <br /> tag follows directly after a paragraph
        m_newParagraph = false;
    if (m_newListItem && tag != "li"_L1)
        // Remove guard for not adding a new line when a <p> tag follows directly after a list item
        m_newListItem = false;

    // Change char format for the following bracket
    QTextCharFormat oldFmt(m_cursor->charFormat());
    if (fmtChanged)
        m_cursor->setCharFormat(m_charFmt);

    // Iterate over children
    for (const HtmlNodePtr &child : node->children)
        renderNode(child);

    // Restore previous char format
    if (fmtChanged) {
        m_charFmt = oldFmt;
        m_cursor->setCharFormat(m_charFmt);
    }

    // Handle closing tags
    if (tag == "p"_L1) {
        if (!m_newListItem)
            finalizeBlock();
        m_newParagraph = false;
    } else if (tag == "ul"_L1) {
        --m_nestedUlTags;
        if (m_nestedUlTags == 0) {
            // Remove list reference
            m_currentList = nullptr;
            m_blockFmt.setObjectIndex(-1);
        }
    } else if (tag == "li"_L1)
        finalizeBlock();
    else if (tag == "h1"_L1 || tag == "h2"_L1 ||
             tag == "h3"_L1 || tag == "h4"_L1) {
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
    QVector<HtmlNodePtr> nodes = HtmlParser__::parse(html);

    // Find <html> tag
    HtmlNodePtr htmlNode = HtmlParser__::findNode(nodes, "html"_L1);
    if (!htmlNode)
        // Put everything inside <html>...</html> tag
        htmlNode = HtmlNode::makeElement("html"_L1, {}, nodes);

    // Setup context for tag styles
    HtmlRenderContext context;
    HtmlNodePtr headNode = HtmlParser__::findNode(htmlNode, "head"_L1);
    if (headNode)
        context.parseHeadNode(headNode);

    // Find <body> tag
    HtmlNodePtr bodyNode = HtmlParser__::findNode(htmlNode, "body"_L1);
    if (!bodyNode)
        // Create bodyNode as root for parsing
        bodyNode = HtmlNode::makeElement("body"_L1, {}, nodes);

    // Render AST onto QTextDocument
    return HtmlRenderer::createDocument(bodyNode, context, parent);
}
