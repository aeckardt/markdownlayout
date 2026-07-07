#include "markdownhtmlparser.h"

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
    const bool closingTag = m_input.at(fwdPos) == QLatin1Char('/');
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
        if (m_input.at(fwdPos) == QLatin1Char('>')) {
            // Valid closing tag found!
            // Advance position for parser
            HtmlScopePtr scope = HtmlScope::makeShared(
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
        QChar ch = m_input.at(fwdPos);
        if (ch.isSpace())
            ++fwdPos;
        else if (ch.isLetter()) {
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
            if (m_input.at(fwdPos) == QLatin1Char('=')) {
                ++fwdPos;

                // Consume whitespaces
                skipWhitespaces(fwdPos);
                if (fwdPos >= m_length)
                    return {};

                QByteArray value;
                ch = m_input.at(fwdPos);
                if (QStringLiteral("\"'").contains(ch)) {
                    QChar quotType = ch;
                    ++fwdPos;
                    while (fwdPos < m_length && m_input.at(fwdPos) != quotType) {
                        value += m_input.at(fwdPos);
                        ++fwdPos;
                    }
                    if (fwdPos >= m_length)
                        return {};
                    ++fwdPos;
                } else
                    value = readAttributeValue(fwdPos);

                if (value.isEmpty())
                    return {};

                attrValue = value;
            }
            attrs.insert(attrName, attrValue);
        } else if (ch == QLatin1Char('/')) {
            // Self closing tag
            if (fwdPos + 1 < m_length && m_input.at(fwdPos + 1) == QLatin1Char('>')) {
                // Valid self closing tag found!
                HtmlScopePtr scope = HtmlScope::makeShared(
                            tagName,
                            HtmlScope::SelfClosingTag,
                            attrs);
                m_pos = fwdPos + 2;
                return scope;
            }
            return {};
        } else if (ch == QLatin1Char('>')) {
            // Closing tag found
            HtmlScopePtr scope = HtmlScope::makeShared(
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

static inline bool isAlpha(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static inline bool isAlphaNumeric(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

QByteArray MarkdownHtmlParser::readIdentifier(int &fwdPos) const
{
    // Read name
    QByteArray identifier;
    identifier += m_input.at(fwdPos);

    char ch = identifier.at(0);

    // The identifier needs to start with an alphabetic character
    if (!isAlpha(ch))
        return {};
    ++fwdPos;
    while (fwdPos < m_length && (isAlphaNumeric(ch) ||
                                 QStringLiteral("-_:").contains(m_input.at(fwdPos)))) {
        identifier += m_input.at(fwdPos);
        ++fwdPos;
    }
    if (fwdPos >= m_length)
        // If the end of the input has been reached, there is no valid HTML tag
        // Therefore return empty string
        return {};
    return identifier;
}

void MarkdownHtmlParser::skipWhitespaces(int &fwdPos) const
{
    QChar ch;
    while (fwdPos < m_length) {
        ch = m_input.at(fwdPos);
        if (ch.isSpace())
            ++fwdPos;
        else
            break;
    }
}

QByteArray MarkdownHtmlParser::readAttributeValue(int &fwdPos) const
{
    QByteArray value;
    while (fwdPos < m_length) {
        const QChar ch = m_input.at(fwdPos);
        if (ch.isLetterOrNumber() || QStringLiteral("-_.:&;,").contains(ch)) {
            value += ch.toLatin1();
            ++fwdPos;
        } else
            return value;
    }
    return {};
}

