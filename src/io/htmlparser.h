#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include "htmlstyle.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QStack>
#include <memory>

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
    enum class Type : int {
        Container,
        HtmlTag,
        Text
    };

    static HtmlNodePtr createContainer()
    { return HtmlNodePtr(new HtmlNode); }

    static HtmlNodePtr createHtmlTag(const HtmlTagPtr &tag, const QVector<HtmlNodePtr> &children = {})
    { return HtmlNodePtr(new HtmlNode(tag, children)); }

    static HtmlNodePtr createText(const QByteArray &text)
    { return HtmlNodePtr(new HtmlNode(text)); }

    HtmlNode(const HtmlNode &) = delete;
    HtmlNode &operator=(const HtmlNode &) = delete;
    ~HtmlNode();

    Type type() const { return m_type; }

    HtmlTagPtr tag() const { Q_ASSERT(m_type == Type::HtmlTag); return *static_cast<HtmlTagPtr *>(m_data); }
    QByteArray text() const { Q_ASSERT(m_type == Type::Text); return *static_cast<QByteArray *>(m_data); }

    QVector<HtmlNodePtr> &children() { return m_children; }
    const QVector<HtmlNodePtr> &children() const { return m_children; }

private:
    HtmlNode()
        : m_type(Type::Container), m_data(nullptr) {}
    HtmlNode(const HtmlTagPtr &tag, const QVector<HtmlNodePtr> &children)
        : m_type(Type::HtmlTag), m_data(new HtmlTagPtr(tag)), m_children(children) {}
    HtmlNode(const QByteArray &text)
        : m_type(Type::Text), m_data(new QByteArray(text)) {}

    Type m_type;

    // Variant depending on m_type
    // Type::Container -> nullptr_t
    // Type::HtmlTag   -> HtmlTagPtr*
    // Type::Text      -> QByteArray*
    void *m_data;

    QVector<HtmlNodePtr> m_children;
};

class HtmlParser
{
public:
    explicit HtmlParser(const QByteArray &html);

    HtmlNodePtr parse();
    HtmlTagPtr parseTag();

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

    const QByteArrayView m_input;
    int m_pos;
    int m_length;

    HtmlNodePtr m_astRoot;
    HtmlNodePtr m_currentParent;
    QStack<HtmlNodePtr> m_openScopeStack;
    int m_textLength;
};

#endif // HTMLPARSER_H
