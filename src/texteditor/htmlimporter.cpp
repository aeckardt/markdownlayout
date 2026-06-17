#include "htmlimporter.h"
#include "htmlimporter_p.h"
#include "htmlstyle.h"
#include "texteditorstyle.h"

#include <QRegularExpression>
#include <QSet>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>

using namespace TextEditorStyle;
using TokenType = HtmlToken::Type;
using NodeType = HtmlNode::Type;

static void appendCodePoint(QString &out, uint codePoint)
{
    if (codePoint == 0 ||
        codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        out.append(QChar(0xFFFD)); // replacement character
        return;
    }

    if (codePoint <= 0xFFFF)
        out.append(QChar(static_cast<ushort>(codePoint)));
    else {
        codePoint -= 0x10000;
        out.append(QChar(static_cast<ushort>(0xD800 + (codePoint >> 10))));
        out.append(QChar(static_cast<ushort>(0xDC00 + (codePoint & 0x3FF))));
    }
}

static QString htmlUnescape(const QString &text)
{
    static const QHash<QString, QString> namedEntities = {
        {QStringLiteral("amp"),  QStringLiteral("&")},
        {QStringLiteral("lt"),   QStringLiteral("<")},
        {QStringLiteral("gt"),   QStringLiteral(">")},
        {QStringLiteral("quot"), QStringLiteral("\"")},
        {QStringLiteral("apos"), QStringLiteral("'")},
        {QStringLiteral("nbsp"), QString(QChar(0x00A0))},

        // Optional common extras:
        {QStringLiteral("ndash"), QString(QChar(0x2013))},
        {QStringLiteral("mdash"), QString(QChar(0x2014))},
        {QStringLiteral("hellip"), QString(QChar(0x2026))}
    };

    QString out;
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

        const QString entity = text.mid(i + 1, semicolon - i - 1);

        if (entity.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            uint codePoint = 0;

            QStringView entityView(entity);

            if (entity.startsWith(QStringLiteral("#x"), Qt::CaseInsensitive))
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
        out.append(QStringView(text).sliced(i, semicolon - i + 1));
        i = semicolon + 1;
    }

    return out;
}

QVector<HtmlNodePtr> HtmlParser::parse(const QString &html)
{
    HtmlParser parser(html);

    // Parse input into tokens
    QVector<HtmlToken> tokens = parser.tokenize();

    // initialize current token position
    int pos = 0;

    // Build and return syntax tree
    // It might not be a tree in a strict sense
    // but rather an array of roots
    return parser.parseChildNodes(tokens, pos);
}

/*
 * findNode recursively searches for a node with the given tag name in the AST.
 *
 * Parameters:
 *     root (HtmlNodePtr): The root of the AST or a list of nodes.
 *     tagName (QString): The tag name to search for (case-insensitive).
 *
 * Returns:
 *     HtmlNodePtr: The first node matching the tag name, or nullptr if not found.
 */
HtmlNodePtr HtmlParser::findNode(const HtmlNodePtr &root, const QString &tagName)
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

HtmlNodePtr HtmlParser::findNode(const QVector<HtmlNodePtr> &nodes, const QString &tagName)
{
    for (const HtmlNodePtr &node : nodes) {
        HtmlNodePtr result = findNode(node, tagName);
        if (result)
            return result;
    }

    return {};
}

QVector<HtmlToken> HtmlParser::tokenize() const
{
    static const QRegularExpression tagPattern(R"((?<tag><[^>]+>)|(?<text>[^<]+))");
    static const QRegularExpression attrPattern(
        R"regex((?<name>[a-zA-Z_:][-a-zA-Z0-9_:.]*)\s*=\s*(?<quote>["'])(?<value>.*?)\k<quote>)regex"
    );
    static const QSet<QString> selfClosingTags = {"br", "img", "hr", "meta"};

    QVector<HtmlToken> tokens;

    auto it = tagPattern.globalMatch(m_input);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        const QString tag = match.captured("tag");
        const QString text = match.captured("text");

        if (!text.isEmpty()) {
            if (QString(text).remove('\n').trimmed().isEmpty())
                continue;

            HtmlToken token;
            token.type = TokenType::Text;
            token.content = htmlUnescape(text);

            tokens.append(token);
        }
        else if (!tag.isEmpty()) {
            if (tag.startsWith("</")) {
                // End tag
                const QString tagName = tag.mid(2, tag.length() - 3)
                                           .trimmed()
                                           .toLower();

                HtmlToken token;
                token.type = TokenType::EndTag;
                token.name = tagName;

                tokens.append(token);
            }
            else {
                const bool isSelfClosing = tag.endsWith("/>");

                QString tagContent;
                if (isSelfClosing)
                    tagContent = tag.mid(1, tag.length() - 3).trimmed();
                else
                    tagContent = tag.mid(1, tag.length() - 2).trimmed();

                QString tagName;
                QString attrString;

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

                    const QString name = attrMatch.captured("name").toLower();
                    const QString value = attrMatch.captured("value");

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


QVector<HtmlNodePtr> HtmlParser::parseChildNodes(const QVector<HtmlToken> &tokens, int &pos, const QString &tagName) const
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
        if (child->name == QStringLiteral("style")) {
            for (const HtmlNodePtr &styleNode : child->children) {
                const QString &styleText = styleNode->content;
                parseCssRules(styleText);
            }
        } else if (child->name == QStringLiteral("meta")) {
            const CssProperties &attrs = child->attrs;
            if (attrs.contains(QStringLiteral("name")) && attrs.contains(QStringLiteral("content")))
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
    QString inlineStr = node->attrs.value("style");
    if (!inlineStr.isEmpty())
        style.insert(parseInlineString(inlineStr));

    return style;
}

CssProperties HtmlRenderContext::parseInlineString(const QString &styleStr)
{
    CssProperties attrs;
    QStringList properties = styleStr.split(';', Qt::SplitBehaviorFlags::SkipEmptyParts);
    for (QString property : properties) {
        property = property.trimmed();
        if (property.isEmpty() || !property.contains(':'))
            continue;

        // Extract name and value by splitting at ':'
        QStringList parts = property.split(':', Qt::SplitBehaviorFlags::SkipEmptyParts);
        if (parts.count() != 2)
            // Ignore mal-formed input
            continue;
        QString name = parts.at(0).trimmed();
        QString value = parts.at(1).trimmed();
        attrs.insert(name, value);
    }
    return attrs;
}

/*
 * Parses a simple CSS string and inserts the properties into m_rules dictionary
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
 * It currently does not handle every legal edge case, for example semicolons inside quotes strings.
 */
void HtmlRenderContext::parseCssRules(const QString &cssText)
{
    static const QRegularExpression rulePattern(R"(([^{]+)\{([^}]+)\})");

    auto it = rulePattern.globalMatch(cssText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();

        const QString selectorBlock = match.captured(1);
        const QString propertiesStr = match.captured(2).trimmed();

        const CssProperties properties = parseProperties(propertiesStr);

        const QStringList selectors =
            selectorBlock.split(QLatin1Char(','), Qt::SkipEmptyParts);

        for (const QString &rawSelector : selectors) {
            const QString selector = rawSelector.trimmed();

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
    const QString tag = node->name.toLower();
    if (tag == QStringLiteral("p")) {
        // Safety guard for not adding a new line directly after a list item
        if (!m_newListItem)
            insertBlock();
        else
            m_newListItem = false;
        m_newParagraph = true;
    } else if (tag == QStringLiteral("br")) {
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
    } else if (tag == QStringLiteral("hr")) {
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
    } else if (tag == QStringLiteral("strong") || tag == QStringLiteral("b")) {
        m_charFmt.setFontWeight(StrongFontWeight);
        fmtChanged = true;
    } else if (tag == QStringLiteral("em") || tag == QStringLiteral("i")) {
        m_charFmt.setFontItalic(true);
        fmtChanged = true;
    } else if (tag == QStringLiteral("ins") || tag == QStringLiteral("u")) {
        m_charFmt.setFontUnderline(true);
        fmtChanged = true;
    } else if (tag == QStringLiteral("a")) {
        m_charFmt.setAnchor(true);
        if (node->attrs.contains(QStringLiteral("href")))
            m_charFmt.setAnchorHref(node->attrs.value(QStringLiteral("href")));
        m_charFmt.setForeground(linkColor());
        fmtChanged = true;
    } else if (tag == QStringLiteral("ul")) {
        // Increase value for more indent (in case more than one ul tag is used)
        ++m_nestedUlTags;
    } else if (tag == QStringLiteral("li")) {
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

        // Set object index for block format
        m_blockFmt.setObjectIndex(m_currentList->objectIndex());

        // Set list style dependent on indent
        if (m_nestedUlTags > 1)
            m_blockFmt.setProperty(QTextFormat::Property::ListStyle, LowerLevelListStyle);
        else
            m_blockFmt.setProperty(QTextFormat::Property::ListStyle, TopLevelListStyle);

        // Add item to list
        m_currentList->add(m_cursor->block());

        // Activate safety guard for not adding a newline with a paragraph directly after
        m_newListItem = true;
    } else if (tag == QStringLiteral("h1") || tag == QStringLiteral("h2") ||
               tag == QStringLiteral("h3") || tag == QStringLiteral("h4")) {
        // Use new line for heading
        insertBlock();

        // Adjust charformat
        const int headingLevel = tag.at(1).digitValue();
        m_blockFmt.setHeadingLevel(headingLevel);
        m_charFmt.setFontWeight(HeadingFontWeight);
        m_charFmt.setProperty(QTextCharFormat::Property::FontSizeAdjustment, 4 - headingLevel);
        fmtChanged = true;
    }

    // Handle styles
    const CssProperties style = m_context.getStyleFor(node);

    // Apply styles to m_charFmt
    fmtChanged = fmtChanged || applyHtmlStyle(style, m_charFmt);

    if (m_newParagraph && tag != QStringLiteral("p"))
        // Remove guard for not adding a new line when a <br /> tag follows directly after a paragraph
        m_newParagraph = false;
    if (m_newListItem && tag != QStringLiteral("li"))
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
    if (tag == QStringLiteral("p")) {
        if (!m_newListItem)
            finalizeBlock();
        m_newParagraph = false;
    } else if (tag == QStringLiteral("ul")) {
        --m_nestedUlTags;
        if (m_nestedUlTags == 0) {
            // Remove list reference
            m_currentList = nullptr;
            m_blockFmt.setObjectIndex(-1);
        }
    } else if (tag == QStringLiteral("li"))
        finalizeBlock();
    else if (tag == QStringLiteral("h1") || tag == QStringLiteral("h2") ||
             tag == QStringLiteral("h3") || tag == QStringLiteral("h4")) {
        finalizeBlock();
        m_blockFmt.setHeadingLevel(0);
    }
}

void HtmlRenderer::insertBlock()
{
    // The first addNewLine called shouldn't add another block
    // Therefore this guard is necessary
    if (m_atBeginning) {
        m_atBeginning = false;
        return;
    }

    // Add new block
    m_cursor->insertBlock();

    // Reset indent
    m_indent = 0;
    m_blockFmt = QTextBlockFormat(defaultBlockFormat());

    // Reset char format (now with default font size)
    m_charFmt = QTextCharFormat(defaultCharFormat());
    m_cursor->setCharFormat(m_charFmt);
}

void HtmlRenderer::finalizeBlock()
{
    // Set block format with indent and heading level before adding new block
    int listIndent = std::max<int>(0, m_nestedUlTags - 1);
    m_blockFmt.setIndent(m_indent + listIndent);
    m_cursor->setBlockFormat(m_blockFmt);
}

QTextDocument *documentFromHtml(const QString &html, QObject *parent)
{
    // Build syntax tree
    QVector<HtmlNodePtr> nodes = HtmlParser::parse(html);

    // Find <html> tag
    HtmlNodePtr htmlNode = HtmlParser::findNode(nodes, "html");
    if (!htmlNode)
        // Put everything inside <html>...</html> tag
        htmlNode = HtmlNode::makeElement("html", {}, nodes);

    // Setup context for tag styles
    HtmlRenderContext context;
    HtmlNodePtr headNode = HtmlParser::findNode(htmlNode, "head");
    if (headNode)
        context.parseHeadNode(headNode);

    // Find <body> tag
    HtmlNodePtr bodyNode = HtmlParser::findNode(htmlNode, "body");
    if (!bodyNode)
        // Create bodyNode as root for parsing
        bodyNode = HtmlNode::makeElement("body", {}, nodes);

    return HtmlRenderer::createDocument(bodyNode, context, parent);
}
