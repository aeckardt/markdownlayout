#ifndef HTMLIMPORTER_P_H
#define HTMLIMPORTER_P_H

#include <QHash>
#include <QVector>
#include <QString>
#include <QStringView>
#include <QTextBlockFormat>
#include <QTextCharFormat>

#include <memory>

#include "htmlstyle.h"

class QObject;
class QTextCursor;
class QTextDocument;
class QTextList;

struct HtmlToken {
    enum class Type {
        StartTag,
        EndTag,
        Text,
        SelfClosingTag
    };

    Type type;
    QString name;  // tag name
    CssProperties attrs;
    QString content;  // for text or comments
};

struct HtmlNode;
typedef std::shared_ptr<HtmlNode> HtmlNodePtr;

struct HtmlNode {
    enum class Type {
        Element,
        Text
    };

    HtmlNode(const QString &name, const CssProperties &attrs, const QVector<HtmlNodePtr> &children = {})
        : type(Type::Element), name(name), attrs(attrs), children(children) {}
    HtmlNode(const QString &content) : type(Type::Text), content(content) {}

    static HtmlNodePtr makeElement(const QString &name, const CssProperties &attrs,
        const QVector<HtmlNodePtr> &children = {})
    { return std::make_shared<HtmlNode>(name, attrs, children); }

    static HtmlNodePtr makeText(const QString &content)
    { return std::make_shared<HtmlNode>(content); }

    Type type;
    QString name;  // tag name if element
    CssProperties attrs;
    QVector<HtmlNodePtr> children;
    QString content;  // only for text nodes
};

class HtmlParser
{
public:
    // Parse the HTML string to generate an array of roots
    // May be one root if everything is encapsulated in <html> tag
    static QVector<HtmlNodePtr> parse(const QString &html);

    // Recursively searches the AST for the first node with the given tag name.
    static HtmlNodePtr findNode(const HtmlNodePtr &root, const QString &tagName);

    // Search in a list rather than a single root element
    static HtmlNodePtr findNode(const QVector<HtmlNodePtr> &nodes, const QString &tagName);

private:
    explicit HtmlParser(const QString &html) : m_input(html) {}

    // Tokenizer
    QVector<HtmlToken> tokenize() const;

    // Parser
    QVector<HtmlNodePtr> parseChildNodes(const QVector<HtmlToken> &tokens, int &pos, const QString &stopTag = {}) const;

    const QStringView m_input;
};

class HtmlRenderContext
{
public:
    void parseHeadNode(const HtmlNodePtr &headNode);
    CssProperties getStyleFor(const HtmlNodePtr &node) const;

private:
    typedef QHash<QString, QString> HtmlMetadata;

    // Parse inline style string (e.g., "color: red; font-weight: bold;") into a dictionary.
    static CssProperties parseInlineString(const QString &styleStr);

    // Parse CSS string into m_rules dictionary
    void parseCssRules(const QString &cssText);

    CssRules m_rules;
    HtmlMetadata m_metadata;
};

class HtmlRenderer
{
public:
    static QTextDocument *createDocument(const HtmlNodePtr &bodyNode, const HtmlRenderContext &context,
                                         QObject *parent = nullptr);

private:
    explicit HtmlRenderer(QTextCursor *cursor, const HtmlRenderContext &context);

    // Render functions
    void renderNode(const HtmlNodePtr &node);
    void insertBlock();
    void finalizeBlock();

    void applyBlockFormatStyle(const CssProperties &style);

    // Render context for style compilation
    const HtmlRenderContext &m_context;

    // Render state variables
    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFmt;
    QTextCharFormat m_charFmt;

    bool m_atBeginning;
    bool m_newParagraph;
    bool m_newListItem;

    int m_indent;
    int m_nestedUlTags;
    QTextList *m_currentList;
};

#endif // HTMLIMPORTER_P_H
