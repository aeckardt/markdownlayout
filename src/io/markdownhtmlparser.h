#ifndef MARKDOWNHTMLPARSER_H
#define MARKDOWNHTMLPARSER_H

#include "htmlstyle.h"

#include <QByteArray>
#include <memory>

class HtmlScope;
typedef std::shared_ptr<HtmlScope> HtmlScopePtr;

class HtmlScope
{
public:
    enum Type {
        OpenTag,
        CloseTag,
        SelfClosingTag
    };

    static HtmlScopePtr createPtr(const QByteArray &tag, Type type, const CssProperties &attrs = {})
    { return HtmlScopePtr(new HtmlScope(tag, type, attrs)); }

    QByteArray tag() const { return m_tag; }
    Type type() const { return m_type; }
    CssProperties attrs() const { return m_attrs; }

private:
    HtmlScope(const QByteArray &tag, Type type, const CssProperties &attrs = {})
        : m_tag(tag), m_type(type), m_attrs(attrs) {}

    QByteArray m_tag;
    Type m_type;
    QHash<QByteArray, QByteArray> m_attrs;
};

class MarkdownHtmlParser
{
public:
    static HtmlScopePtr parseTag(const QByteArray &html);

private:
    explicit MarkdownHtmlParser(const QByteArray &html);

    HtmlScopePtr tryParseHtmlTag();
    bool readIdentifier(QByteArray &identifier);
    void skipWhitespaces();
    bool readAttributeValue(QByteArray &value);

    int m_pos;
    int m_length;
    const QByteArray &m_input;
};

#endif // MARKDOWNHTMLPARSER_H
