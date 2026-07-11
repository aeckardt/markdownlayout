#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include "htmlstyle.h"

#include <QByteArray>
#include <QStack>
#include <QVector>
#include <memory>
#include <variant>

class HtmlTag;
typedef std::shared_ptr<HtmlTag> HtmlTagPtr;

class HtmlTag
{
public:
    enum Type {
        OpenTag,
        CloseTag,
        SelfClosingTag
    };

    static HtmlTagPtr create(const QByteArray &tag, Type type, const CssProperties &attrs = {})
    { return HtmlTagPtr(new HtmlTag(tag, type, attrs)); }

    HtmlTag(const HtmlTag &) = delete;
    HtmlTag &operator=(const HtmlTag &) = delete;

    QByteArray name() const { return m_tagName; }
    Type type() const { return m_type; }
    CssProperties attrs() const { return m_attrs; }

private:
    HtmlTag(const QByteArray &tag, Type type, const CssProperties &attrs = {})
        : m_tagName(tag), m_type(type), m_attrs(attrs) {}

    QByteArray m_tagName;
    Type m_type;
    QHash<QByteArray, QByteArray> m_attrs;
};

class HtmlNode;
typedef std::shared_ptr<HtmlNode> HtmlNodePtr;

class HtmlNode
{
public:
    enum class Type : size_t {
        Container = 0,
        HtmlTag   = 1,
        Text      = 2
    };

    static HtmlNodePtr createContainer()
    { return HtmlNodePtr(new HtmlNode); }

    static HtmlNodePtr createHtmlTag(const HtmlTagPtr &tag, const QVector<HtmlNodePtr> &children = {})
    { return HtmlNodePtr(new HtmlNode(tag, children)); }

    static HtmlNodePtr createText(const QByteArray &text)
    { return HtmlNodePtr(new HtmlNode(text)); }

    HtmlNode(const HtmlNode &) = delete;
    HtmlNode &operator=(const HtmlNode &) = delete;

    Type type() const { return static_cast<Type>(m_data.index()); }

    HtmlTagPtr tag() const { Q_ASSERT(type() == Type::HtmlTag); return std::get<HtmlTagPtr>(m_data); }
    QByteArray text() const { Q_ASSERT(type() == Type::Text); return std::get<QByteArray>(m_data); }

    QVector<HtmlNodePtr> &children() { return m_children; }
    const QVector<HtmlNodePtr> &children() const { return m_children; }

private:
    HtmlNode() = default;
    HtmlNode(const HtmlTagPtr &tag, const QVector<HtmlNodePtr> &children)
        : m_data(tag), m_children(children) {}
    HtmlNode(const QByteArray &text)
        : m_data(text) {}

    std::variant<std::monostate, HtmlTagPtr, QByteArray> m_data;
    QVector<HtmlNodePtr> m_children;
};

class HtmlParser
{
public:
    explicit HtmlParser(const QByteArray &html);

    HtmlNodePtr parse();
    HtmlTagPtr parseTag();

    HtmlNodePtr findNode(const HtmlNodePtr &root, const QByteArray &tagName);

    // Common parsing helpers
    static inline bool isAlpha(const char ch);
    static inline bool isAlphaNumeric(const char ch);
    static inline bool isWhitespace(const char ch);

    static QByteArray htmlUnescape(const QByteArrayView &text);

private:
    bool readIdentifier(int &fwdPos, QByteArray &identifier);
    void skipWhitespaces(int &fwdPos);
    bool readAttributeValue(int &fwdPos, QByteArray &value);

    HtmlNodePtr findOpeningTag(HtmlNodePtr node);
    void pushScopeTag(HtmlNodePtr node);
    HtmlNodePtr popScopeTag();
    void integrateNode(HtmlNodePtr node);
    void flattenNode(HtmlNodePtr node);
    void flushText();

    const QByteArray m_input;
    int m_pos;
    int m_length;

    HtmlNodePtr m_astRoot;
    HtmlNodePtr m_currentParent;
    QStack<HtmlNodePtr> m_openScopeStack;
    int m_textLength;
};

inline bool HtmlParser::isAlpha(const char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

inline bool HtmlParser::isAlphaNumeric(const char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

inline bool HtmlParser::isWhitespace(const char ch)
{
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\f';
}

#endif // HTMLPARSER_H
