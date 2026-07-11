#include "htmlparser.h"

#include <QByteArrayView>

// Define parsing helpers
static bool isAlpha(const char ch);
static bool isAlphaNumeric(const char ch);
static bool isHtmlSpace(const char ch);
static void appendUtf8CodePoint(QByteArray &out, uint codePoint);
static QByteArray htmlUnescape(const QByteArray &text);

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
    // Initialize data
    m_astRoot = HtmlNode::createContainer();
    m_currentParent = m_astRoot;
    m_openScopeStack.clear();

    m_pos = 0;
    m_length = m_input.size();
    m_text.clear();

    // Iterate through input string
    while (m_pos < m_length) {
        const char ch = m_input.at(m_pos);
        switch (ch) {
        case '<': {
            flushText();

            // Try to parse HTML tag
            int start = m_pos;
            HtmlTagPtr tag = parseTag();
            if (!tag) {
                m_text += ch;
                ++m_pos;
                continue;
            }

            int length = m_pos - start;
            HtmlNodePtr marker = HtmlNode::createHtmlTag(start, length, tag);

            switch (tag->type()) {
            case HtmlTag::OpenTag:
                pushScopeMarker(marker);
                break;
            case HtmlTag::CloseTag: {
                HtmlNodePtr openMarker = findOpeningMarker(marker);
                if (!openMarker) {
                    integrateMarkerAsText(marker);
                    continue;
                }
                integrateNode(openMarker);
                break;
            }
            case HtmlTag::SelfClosingTag:
                integrateNode(marker);
                break;
            default:
                ;
            }

            continue;
        }
        default:
            m_text += ch;
            ++m_pos;
        }
    }

    flushText();
    while (!m_openScopeStack.isEmpty()) {
        HtmlNodePtr marker = popScopeMarker();
        integrateMarkerAsText(marker);
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
            HtmlTagPtr scope = HtmlTag::create(
                        tagName,
                        HtmlTag::CloseTag);
            m_pos = fwdPos + 1;
            return scope;
        }
        // Condition for valid closing tag violated
        // '>' expected, but not found
        return {};
    }

    // Parse attributes or end of tag
    CssProperties attrs;
    while (fwdPos < m_length) {
        char ch = m_input.at(fwdPos);
        if (isHtmlSpace(ch))
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
            attrs.insert(attrName, attrValue);
        } else if (ch == '/') {
            // Self closing tag
            if (fwdPos + 1 < m_length && m_input.at(fwdPos + 1) == '>') {
                // Valid self closing tag found!
                HtmlTagPtr scope = HtmlTag::create(
                            tagName,
                            HtmlTag::SelfClosingTag,
                            attrs);
                m_pos = fwdPos + 2;
                return scope;
            }
            return {};
        } else if (ch == '>') {
            // Closing tag found
            HtmlTagPtr scope = HtmlTag::create(
                        tagName,
                        HtmlTag::OpenTag,
                        attrs);
            m_pos = fwdPos + 1;
            return scope;
        } else
            // Other characters are not allowed here:
            return {};
    }

    return {};
}

bool HtmlParser::readIdentifier(int &fwdPos, QByteArray &identifier)
{
    // Read first character
    identifier = QByteArray(1, m_input.at(fwdPos));

    // The identifier needs to start with an alphabetic character
    if (!isAlpha(identifier.at(0)))
        return false;

    ++fwdPos;
    while (fwdPos < m_length) {
        static constexpr QByteArrayView specialChars("-_:");
        const char ch = m_input.at(fwdPos);
        if (isAlphaNumeric(ch) || specialChars.contains(ch)) {
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
        if (isHtmlSpace(ch))
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
            if (isHtmlSpace(ch) || delimiterChars.contains(ch))
                break;
            value += ch;
            ++fwdPos;
        }
        if (value.isEmpty())
            return false;
    }

    return true;
}

HtmlNodePtr HtmlParser::findOpeningMarker(HtmlNodePtr marker)
{
    int index = m_openScopeStack.size() - 1;
    while (index >= 0) {
        const HtmlNodePtr &openMarker = m_openScopeStack[index];
        const QByteArray &openTag = openMarker->tag()->name();
        const QByteArray &closeTag = marker->tag()->name();
        if (openTag == closeTag)
            break;
        --index;
    }
    if (index < 0)
        return {};

    while (true) {
        HtmlNodePtr poppedMarker = popScopeMarker();
        const QString &poppedTag = poppedMarker->tag()->name();
        const QString &closeTag = marker->tag()->name();
        if (poppedTag == closeTag)
            return poppedMarker;

        // Interpret un-matched markers as text
        integrateMarkerAsText(poppedMarker);
    }
}
void HtmlParser::pushScopeMarker(HtmlNodePtr marker)
{
    m_currentParent = marker;

    // Push marker to the stack
    m_openScopeStack.append(marker);
}

HtmlNodePtr HtmlParser::popScopeMarker()
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

void HtmlParser::integrateMarkerAsText(HtmlNodePtr marker)
{
    // Append text node with marker characters to the tree
    integrateNode(HtmlNode::createText(marker->start(), marker->length()));

    // Flatten the structure by moving all the children from the floating
    // marker node to the current parent node
    HtmlNodePtr node = m_currentParent;
    node->children() += std::move(marker->children());
    marker->children().clear();

    // The inline node from parsing is not needed anymore
    marker.reset();
}

void HtmlParser::flushText()
{
    if (m_text.isEmpty())
        return;

    // Append new text node to the tree
    integrateNode(HtmlNode::createText(m_pos - m_text.length(), m_text.length()));

    // Clear text
    m_text.clear();
}

const QByteArray HtmlParser::text(HtmlNodePtr textNode) const
{
    Q_ASSERT(textNode->type() == HtmlNode::Type::Text);
    return htmlUnescape(m_input.mid(textNode->start(), textNode->length()));
}

static inline bool isAlpha(const char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static inline bool isAlphaNumeric(const char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

static inline bool isHtmlSpace(const char ch)
{
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\f';
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

static QByteArray htmlUnescape(const QByteArray &text)
{
    static const QHash<QByteArray, QByteArray> namedEntities = {
        {"amp",  "&"},
        {"lt",   "<"},
        {"gt",   ">"},
        {"quot", "\""},
        {"apos", "'"},
        // TODO: Replace with UTF-8 sequence
//        {"nbsp", QByteArray(QChar(0x00A0))},

//        // Optional common extras:
//        {"ndash", QByteArray(QChar(0x2013))},
//        {"mdash", QByteArray(QChar(0x2014))},
//        {"hellip", QByteArray(QChar(0x2026))}
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

        const qsizetype semicolon = text.indexOf(';', i + 1);

        if (semicolon < 0) {
            out.append(text.at(i));
            ++i;
            continue;
        }

        const QByteArray entity = text.mid(i + 1, semicolon - i - 1);

        if (entity.startsWith('#')) {
            bool ok = false;
            uint codePoint = 0;

            QByteArrayView entityView(entity);

            if (entity.startsWith("#x") || entity.startsWith("#X"))
                codePoint = entityView.sliced(2).toUInt(&ok, 16);
            else
                codePoint = entityView.sliced(1).toUInt(&ok, 10);

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
        out.append(QByteArrayView(text).sliced(i, semicolon - i + 1));
        i = semicolon + 1;
    }

    return out;
}
