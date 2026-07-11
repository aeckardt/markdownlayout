#include "htmlparser.h"

#include <QByteArrayView>
#include <QHash>
#include <QSet>

static int hexDigitValue(const char ch);
static void appendUtf8CodePoint(QByteArray &out, uint codePoint);
static bool parseCodePoint(const QByteArrayView &text, int base, uint &codePoint);

HtmlParser::HtmlParser(const QByteArray &html)
    : m_input(html),
      m_pos(0),
      m_length(html.length()),
      m_astRoot(nullptr),
      m_currentParent(nullptr)
{
}

HtmlNodePtr HtmlParser::parse()
{
    m_astRoot = HtmlNode::createContainer();
    m_currentParent = m_astRoot;
    m_openScopeStack.clear();

    m_pos = 0;
    m_length = m_input.size();
    m_textLength = 0;

    // Iterate through input string
    while (m_pos < m_length) {
        if (m_input.at(m_pos) == '<') {
            flushText();

            // Try to parse HTML tag
            HtmlTagPtr tag = parseTag();
            if (!tag) {
                ++m_pos;
                ++m_textLength;
                continue;
            }

            HtmlNodePtr node = HtmlNode::createHtmlTag(tag);

            switch (tag->type()) {
            case HtmlTag::OpenTag:
                pushScopeTag(node);
                break;
            case HtmlTag::CloseTag: {
                HtmlNodePtr openNode = findOpeningTag(node);
                if (!openNode) {
                    flattenNode(node);
                    continue;
                }
                integrateNode(openNode);
                break;
            }
            case HtmlTag::SelfClosingTag:
                integrateNode(node);
                break;
            default:
                ;
            }

            continue;
        } else {
            ++m_pos;
            ++m_textLength;
        }
    }

    flushText();
    while (!m_openScopeStack.isEmpty()) {
        HtmlNodePtr node = popScopeTag();
        flattenNode(node);
    }

    return m_astRoot;
}

HtmlTagPtr HtmlParser::parseTag()
{
    // Condition for entering this method:
    // At m_pos there is a "<".
    Q_ASSERT(m_pos < m_length && m_input.at(m_pos) == '<');

    // Advance past '<'
    int fwdPos = m_pos + 1;
    if (fwdPos >= m_length)
        return {};

    // Look for optional closing slash
    const bool closingTag = m_input.at(fwdPos) == '/';
    if (closingTag)
        ++fwdPos;
    if (fwdPos >= m_length)
        return {};

    // Read tag name
    // Note: Whitespaces before the tag name are not allowed
    QByteArray tagName;
    if (!readIdentifier(fwdPos, tagName))
        return {};

    // Consume whitespaces
    skipWhitespaces(fwdPos);
    if (fwdPos >= m_length)
        // Return if the input has ended unexpectedly
        return {};

    // If the tag is also closing tag, check for '>'
    if (closingTag) {
        if (m_input.at(fwdPos) == '>') {
            // Valid closing tag found!
            // Advance position for parser
            HtmlTagPtr tag = HtmlTag::create(
                        tagName,
                        HtmlTag::CloseTag);
            m_pos = fwdPos + 1;
            return tag;
        }
        // Condition for valid closing tag violated
        // '>' expected, but not found
        return {};
    }

    // Parse attributes or end of tag
    CssProperties attrs;
    while (fwdPos < m_length) {
        char ch = m_input.at(fwdPos);
        if (isWhitespace(ch))
            ++fwdPos;
        else if (isAlpha(ch)) {
            // Parse attribute
            // Read attribute name
            QByteArray attrName;
            if (!readIdentifier(fwdPos, attrName))
                return {};

            // Consume whitespaces
            skipWhitespaces(fwdPos);
            if (fwdPos >= m_length)
                return {};

            // Check for equality sign
            QByteArray attrValue;
            if (m_input.at(fwdPos) == '=') {
                ++fwdPos;

                // Consume whitespaces
                skipWhitespaces(fwdPos);
                if (fwdPos >= m_length)
                    return {};

                if (!readAttributeValue(fwdPos, attrValue))
                    return {};
            }
            // An empty attrValue can be interpreted as "true" for boolean fields
            attrs.insert(attrName, htmlUnescape(attrValue));
        } else if (ch == '/') {
            // Self closing tag
            if (fwdPos + 1 < m_length && m_input.at(fwdPos + 1) == '>') {
                // Valid self closing tag found!
                HtmlTagPtr tag = HtmlTag::create(
                            tagName,
                            HtmlTag::SelfClosingTag,
                            attrs);
                m_pos = fwdPos + 2;
                return tag;
            }
            return {};
        } else if (ch == '>') {
            // Closing tag found
            static const QSet<QByteArray> selfClosingTags = {"br", "img", "hr", "meta"};
            HtmlTag::Type type = selfClosingTags.contains(tagName)
                    ? HtmlTag::SelfClosingTag
                    : HtmlTag::OpenTag;
            HtmlTagPtr tag = HtmlTag::create(
                        tagName,
                        type,
                        attrs);
            m_pos = fwdPos + 1;
            return tag;
        } else
            // Other characters are not allowed here:
            return {};
    }

    return {};
}

bool HtmlParser::readIdentifier(int &fwdPos, QByteArray &identifier)
{
    // Read first character
    identifier = QByteArray(1, m_input.at(fwdPos)).toLower();

    // The identifier needs to start with an alphabetic character
    if (!isAlpha(identifier.at(0)))
        return false;

    ++fwdPos;
    while (fwdPos < m_length) {
        static constexpr QByteArrayView specialChars("-_:");
        const char ch = m_input.at(fwdPos);
        if (isAlphaNumeric(ch) || specialChars.contains(ch)) {
            if (ch >= 'A' && ch <= 'Z')
                // Append lowercase char
                identifier += ch - 'A' + 'a';
            else
                identifier += ch;
            ++fwdPos;
        } else
            break;
    }
    if (fwdPos >= m_length)
        // If the end of the input has been reached, there is no valid HTML tag
        return false;

    return true;
}

void HtmlParser::skipWhitespaces(int &fwdPos)
{
    while (fwdPos < m_length) {
        const char ch = m_input.at(fwdPos);
        if (isWhitespace(ch))
            ++fwdPos;
        else
            break;
    }
}

bool HtmlParser::readAttributeValue(int &fwdPos, QByteArray &value)
{
    const char ch = m_input.at(fwdPos);
    if (ch == '"' || ch == '\'') {
        const char quotType = ch;
        ++fwdPos;
        while (fwdPos < m_length && m_input.at(fwdPos) != quotType) {
            value += m_input.at(fwdPos);
            ++fwdPos;
        }
        if (fwdPos >= m_length)
            return false;
        ++fwdPos;
    } else {
        while (fwdPos < m_length) {
            static constexpr QByteArrayView delimiterChars("><\"'`=");
            const char ch = m_input.at(fwdPos);
            if (isWhitespace(ch) || delimiterChars.contains(ch))
                break;
            value += ch;
            ++fwdPos;
        }
        if (value.isEmpty())
            return false;
    }

    return true;
}

HtmlNodePtr HtmlParser::findOpeningTag(HtmlNodePtr node)
{
    int index = m_openScopeStack.size() - 1;
    while (index >= 0) {
        const HtmlNodePtr &openNode = m_openScopeStack[index];
        const QByteArray openTag = openNode->tag()->name();
        const QByteArray closeTag = node->tag()->name();
        if (openTag == closeTag)
            break;
        --index;
    }
    if (index < 0)
        return {};

    while (true) {
        HtmlNodePtr poppedNode = popScopeTag();
        const QByteArray poppedTag = poppedNode->tag()->name();
        const QByteArray closeTag = node->tag()->name();
        if (poppedTag == closeTag)
            return poppedNode;

        // Remove un-matched node
        flattenNode(poppedNode);
    }
}
void HtmlParser::pushScopeTag(HtmlNodePtr node)
{
    m_currentParent = node;

    // Push node to the stack
    m_openScopeStack.append(node);
}

HtmlNodePtr HtmlParser::popScopeTag()
{
    HtmlNodePtr popped = m_openScopeStack.pop();

    // Update current parent
    if (!m_openScopeStack.isEmpty())
        m_currentParent = m_openScopeStack.top();
    else
        m_currentParent = m_astRoot;

    return popped;
}

void HtmlParser::integrateNode(HtmlNodePtr node)
{
    // Append the text node to the current parent node
    m_currentParent->children().append(node);
}

void HtmlParser::flattenNode(HtmlNodePtr node)
{
    // Flatten the structure by moving all the children from the floating
    // node to the current parent node
    m_currentParent->children() += std::move(node->children());

    // The floating node will be removed after the reference counter drops to 0
}

void HtmlParser::flushText()
{
    if (m_textLength == 0)
        return;

    QByteArray rawText =
            m_input.sliced(m_pos - m_textLength, m_textLength);
    if (rawText.replace('\n', "").trimmed().isEmpty()) {
        m_textLength = 0;
        return;
    }

    // Append new text node to the tree
    integrateNode(HtmlNode::createText(htmlUnescape(m_input.sliced(m_pos - m_textLength, m_textLength))));

    // Clear text
    m_textLength = 0;
}

/* findNode recursively searches for a node with the given tag name in the AST.
 *
 * Parameters:
 *     root (HtmlNodePtr): The root of the AST or a list of nodes.
 *     tagName (QByteArray): The tag name to search for (case-insensitive).
 *
 * Returns:
 *     HtmlNodePtr: The first node matching the tag name, or nullptr if not found. */
HtmlNodePtr HtmlParser::findNode(const HtmlNodePtr &root, const QByteArray &tagName)
{
    // Check if the current node matches the tag.
    if (root->type() == HtmlNode::Type::HtmlTag && root->tag()->name() == tagName)
        return root;

    // Otherwise, search recursively in the children.
    const QVector<HtmlNodePtr> &children = root->children();
    for (const HtmlNodePtr &child : children) {
        HtmlNodePtr result = findNode(child, tagName);
        if (result)
            return result;
    }

    return {};
}

static inline int hexDigitValue(const char ch)
{
    switch (ch) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return ch - '0';
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
        return 10 + ch - 'a';
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
        return 10 + ch - 'A';
    default:
        return -1;
    }
}

static void appendUtf8CodePoint(QByteArray &out, uint codePoint)
{
    if (codePoint == 0
            || codePoint > 0x10FFFF
            || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        out.append("\xEF\xBF\xBD", 3); // U+FFFD replacement character in UTF-8
        return;
    }

    if (codePoint <= 0x7F)
        out.append(static_cast<char>(codePoint));
    else if (codePoint <= 0x7FF) {
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

static bool parseCodePoint(const QByteArrayView &text, int base, uint &codePoint)
{
    if (text.isEmpty())
        return false;

    uint value = 0;

    for (char ch : text) {
        int digit = -1;

        if (base == 10) {
            if (ch >= '0' && ch <= '9')
                digit = ch - '0';
        } else if (base == 16)
            digit = hexDigitValue(ch);

        if (digit < 0 || digit >= base)
            return false;

        if (value <= 0x10FFFF)
            value = value * base + static_cast<uint>(digit);

        if (value > 0x10FFFF)
            value = 0x110000; // valid number syntax, invalid Unicode code point
    }

    codePoint = value;
    return true;
}

QByteArray HtmlParser::htmlUnescape(const QByteArrayView &text)
{
    static const QHash<QByteArrayView, QByteArray> namedEntities = {
        {"amp",  "&"},
        {"lt",   "<"},
        {"gt",   ">"},
        {"quot", "\""},
        {"apos", "'"},
        {"nbsp",  QByteArray("\xC2\xA0", 2)},
        {"ndash", QByteArray("\xE2\x80\x93", 3)},
        {"mdash", QByteArray("\xE2\x80\x94", 3)},
        {"hellip", QByteArray("\xE2\x80\xA6", 3)}
    };

    QByteArray out;
    out.reserve(text.size());

    qsizetype i = 0;
    while (i < text.size()) {
        if (text.at(i) != '&') {
            out.append(text.at(i));
            ++i;
            continue;
        }

        const qsizetype semicolon = text.indexOf(';', i + 1);
        if (semicolon < 0) {
            out.append(text.at(i));
            ++i;
            continue;
        }

        const QByteArrayView entity = text.sliced(i + 1, semicolon - i - 1);

        if (entity.startsWith('#')) {
            bool ok = false;
            uint codePoint = 0;

            if (entity.size() >= 2 &&
                (entity.at(1) == 'x' || entity.at(1) == 'X')) {
                ok = parseCodePoint(entity.sliced(2), 16, codePoint);
            } else
                ok = parseCodePoint(entity.sliced(1), 10, codePoint);

            if (ok) {
                appendUtf8CodePoint(out, codePoint);
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
        out.append(text.sliced(i, semicolon - i + 1));
        i = semicolon + 1;
    }

    return out;
}
