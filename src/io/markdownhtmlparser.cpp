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
    const QByteArray tagName = readIdentifier(fwdPos);
    if (tagName.isEmpty())
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
            HtmlScopePtr scope = HtmlScope::createPtr(
                        tagName,
                        HtmlScope::CloseTag);
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
            const QByteArray attrName = readIdentifier(fwdPos);
            if (attrName.isEmpty())
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

                bool ok;
                QByteArray value = readAttributeValue(fwdPos, ok);
                if (!ok)
                    return {};

                attrValue = value;
            }
            attrs.insert(attrName, attrValue);
        } else if (ch == '/') {
            // Self closing tag
            if (fwdPos + 1 < m_length && m_input.at(fwdPos + 1) == '>') {
                // Valid self closing tag found!
                HtmlScopePtr scope = HtmlScope::createPtr(
                            tagName,
                            HtmlScope::SelfClosingTag,
                            attrs);
                m_pos = fwdPos + 2;
                return scope;
            }
            return {};
        } else if (ch == '>') {
            // Closing tag found
            HtmlScopePtr scope = HtmlScope::createPtr(
                        tagName,
                        HtmlScope::OpenTag,
                        attrs);
            m_pos = fwdPos + 1;
            return scope;
        } else
            // Other characters are not allowed here:
            return {};
    }

    return {};
}

QByteArray MarkdownHtmlParser::readIdentifier(int &fwdPos) const
{
    // Read name
    QByteArray identifier;
    identifier += m_input.at(fwdPos);

    // The identifier needs to start with an alphabetic character
    if (!isAlpha(identifier.at(0)))
        return {};
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
        // Therefore return empty string
        return {};
    return identifier;
}

void MarkdownHtmlParser::skipWhitespaces(int &fwdPos) const
{
    while (fwdPos < m_length) {
        const char ch = m_input.at(fwdPos);
        if (isHtmlSpace(ch))
            ++fwdPos;
        else
            break;
    }
}

QByteArray MarkdownHtmlParser::readAttributeValue(int &fwdPos, bool &ok) const
{
    QByteArray value;
    const char ch = m_input.at(fwdPos);
    if (ch == '"' || ch == '\'') {
        const char quotType = ch;
        ++fwdPos;
        while (fwdPos < m_length && m_input.at(fwdPos) != quotType) {
            value += m_input.at(fwdPos);
            ++fwdPos;
        }
        if (fwdPos >= m_length) {
            ok = false;
            return {};
        }
        ++fwdPos;
    } else {
        while (fwdPos < m_length) {
            static constexpr QByteArrayView delimiterChars("><\"'`=");
            const char ch = m_input.at(fwdPos);
            if (isSpace(ch) || delimiterChars.contains(ch))
                break;
            value += ch;
            ++fwdPos;
        }
        if (value.isEmpty()) {
            ok = false;
            return {};
        }
    }

    ok = true;
    return value;
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
