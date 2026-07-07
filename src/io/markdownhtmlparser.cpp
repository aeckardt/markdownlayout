#include "markdownhtmlparser.h"

#include <QByteArrayView>

// Define parsing helpers
static bool isAlpha(const char ch);
static bool isAlphaNumeric(const char ch);
static bool isHtmlSpace(const char ch);

HtmlScopePtr MarkdownHtmlParser::parseTag(const QByteArray &html)
{
    MarkdownHtmlParser htmlParser(html);
    return htmlParser.tryParseHtmlTag();
}

MarkdownHtmlParser::MarkdownHtmlParser(const QByteArray &html)
    : m_pos(0),
      m_length(html.length()),
      m_input(html)
{
}

HtmlScopePtr MarkdownHtmlParser::tryParseHtmlTag()
{
    // Condition for entering this method:
    // At m_pos there is a "<".

    // Advance past '<'
    ++m_pos;
    if (m_pos >= m_length)
        return {};

    // Look for optional closing slash
    const bool closingTag = m_input.at(m_pos) == '/';
    if (closingTag)
        ++m_pos;
    if (m_pos >= m_length)
        return {};

    // Read tag name
    // Note: Whitespaces before the tag name are not allowed
    QByteArray tagName;
    if (!readIdentifier(tagName))
        return {};

    // Consume whitespaces
    skipWhitespaces();
    if (m_pos >= m_length)
        // Return if the input has ended unexpectedly
        return {};

    // If the tag is also closing tag, check for '>'
    if (closingTag) {
        if (m_input.at(m_pos) == '>') {
            // Valid closing tag found!
            // Advance position for parser
            HtmlScopePtr scope = HtmlScope::createPtr(
                        tagName,
                        HtmlScope::CloseTag);
            m_pos = m_pos + 1;
            return scope;
        }
        // Condition for valid closing tag violated
        // '>' expected, but not found
        return {};
    }

    // Parse attributes or end of tag
    CssProperties attrs;
    while (m_pos < m_length) {
        char ch = m_input.at(m_pos);
        if (isHtmlSpace(ch))
            ++m_pos;
        else if (isAlpha(ch)) {
            // Parse attribute
            // Read attribute name
            QByteArray attrName;
            if (!readIdentifier(attrName))
                return {};

            // Consume whitespaces
            skipWhitespaces();
            if (m_pos >= m_length)
                return {};

            // Check for equality sign
            QByteArray attrValue;
            if (m_input.at(m_pos) == '=') {
                ++m_pos;

                // Consume whitespaces
                skipWhitespaces();
                if (m_pos >= m_length)
                    return {};

                if (!readAttributeValue(attrValue))
                    return {};
            }
            // An empty attrValue can be interpreted as "true" for boolean fields
            attrs.insert(attrName, attrValue);
        } else if (ch == '/') {
            // Self closing tag
            if (m_pos + 1 < m_length && m_input.at(m_pos + 1) == '>') {
                // Valid self closing tag found!
                HtmlScopePtr scope = HtmlScope::createPtr(
                            tagName,
                            HtmlScope::SelfClosingTag,
                            attrs);
                m_pos = m_pos + 2;
                return scope;
            }
            return {};
        } else if (ch == '>') {
            // Closing tag found
            HtmlScopePtr scope = HtmlScope::createPtr(
                        tagName,
                        HtmlScope::OpenTag,
                        attrs);
            m_pos = m_pos + 1;
            return scope;
        } else
            // Other characters are not allowed here:
            return {};
    }

    return {};
}

bool MarkdownHtmlParser::readIdentifier(QByteArray &identifier)
{
    // Read first character
    identifier = QByteArray(1, m_input.at(m_pos));

    // The identifier needs to start with an alphabetic character
    if (!isAlpha(identifier.at(0)))
        return false;

    ++m_pos;
    while (m_pos < m_length) {
        static constexpr QByteArrayView specialChars("-_:");
        const char ch = m_input.at(m_pos);
        if (isAlphaNumeric(ch) || specialChars.contains(ch)) {
            identifier += ch;
            ++m_pos;
        } else
            break;
    }
    if (m_pos >= m_length)
        // If the end of the input has been reached, there is no valid HTML tag
        return false;

    return true;
}

void MarkdownHtmlParser::skipWhitespaces()
{
    while (m_pos < m_length) {
        const char ch = m_input.at(m_pos);
        if (isHtmlSpace(ch))
            ++m_pos;
        else
            break;
    }
}

bool MarkdownHtmlParser::readAttributeValue(QByteArray &value)
{
    const char ch = m_input.at(m_pos);
    if (ch == '"' || ch == '\'') {
        const char quotType = ch;
        ++m_pos;
        while (m_pos < m_length && m_input.at(m_pos) != quotType) {
            value += m_input.at(m_pos);
            ++m_pos;
        }
        if (m_pos >= m_length)
            return false;
        ++m_pos;
    } else {
        while (m_pos < m_length) {
            static constexpr QByteArrayView delimiterChars("><\"'`=");
            const char ch = m_input.at(m_pos);
            if (isHtmlSpace(ch) || delimiterChars.contains(ch))
                break;
            value += ch;
            ++m_pos;
        }
        if (value.isEmpty())
            return false;
    }

    return true;
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
