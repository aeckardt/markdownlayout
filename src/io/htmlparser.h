#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include "htmlstyle.h"

#include <QByteArray>
#include <QStack>
#include <memory>

class HtmlTag;
typedef std::shared_ptr<HtmlTag> HtmlTagPtr;

class HtmlNode;
typedef std::shared_ptr<HtmlNode> HtmlNodePtr;

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

class HtmlNode
{
public:
    enum class Type {
        Container,
        HtmlTag,
        Text
    };

    static HtmlNodePtr createContainer()
    { return HtmlNodePtr(new HtmlNode(Type::Container, -1, -1)); }

    static HtmlNodePtr createHtmlTag(int start, int length, const HtmlTagPtr &tag,
                                     const QVector<HtmlNodePtr> &children = {})
    { return HtmlNodePtr(new HtmlNode(Type::HtmlTag, start, length, tag, children)); }

    static HtmlNodePtr createText(int start, int length)
    { return HtmlNodePtr(new HtmlNode(Type::Text, start, length)); }

    Type type() const { return m_type; }
    int start() const { return m_start; }
    int length() const { return m_length; }
    HtmlTagPtr tag() const { Q_ASSERT(m_type == Type::HtmlTag); return m_tag; }
    QVector<HtmlNodePtr> &children() { return m_children; }
    QVector<HtmlNodePtr> children() const { return m_children; }

private:
    HtmlNode(Type type, int start, int length, const HtmlTagPtr &tag = {},
             const QVector<HtmlNodePtr> &children = {})
        : m_type(type), m_start(start), m_length(length), m_tag(tag), m_children(children) {}

    Type m_type;
    int m_start;
    int m_length;
    HtmlTagPtr m_tag;
    QVector<HtmlNodePtr> m_children;
};

class HtmlParser
{
public:
    explicit HtmlParser(const QByteArray &html);

    HtmlNodePtr parse();
    HtmlTagPtr parseTag();

    const QByteArray text(HtmlNodePtr textNode) const;

private:
    bool readIdentifier(int &fwdPos, QByteArray &identifier);
    void skipWhitespaces(int &fwdPos);
    bool readAttributeValue(int &fwdPos, QByteArray &value);

    HtmlNodePtr findOpeningMarker(HtmlNodePtr marker);
    void pushScopeMarker(HtmlNodePtr marker);
    HtmlNodePtr popScopeMarker();
    void integrateNode(HtmlNodePtr node);
    void integrateMarkerAsText(HtmlNodePtr marker);
    void flushText();

    const QByteArray m_input;
    int m_pos;
    int m_length;

    HtmlNodePtr m_astRoot;
    HtmlNodePtr m_currentParent;
    QStack<HtmlNodePtr> m_openScopeStack;
    QByteArray m_text;
};

#endif // HTMLPARSER_H
