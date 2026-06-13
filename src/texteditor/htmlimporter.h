#ifndef HTMLIMPORTER_H
#define HTMLIMPORTER_H

#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QMap>
#include <memory>

#include "htmlstyle.h"

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

    Type type;
    QString name;  // tag name if element
    CssProperties attrs;
    QVector<HtmlNodePtr> children;
    QString content;  // only for text nodes

    inline HtmlNode(const QString &name, const CssProperties &attrs, const QVector<HtmlNodePtr> &children = {})
        : type(Type::Element), name(name), attrs(attrs), children(children) {}
    inline HtmlNode(const QString &content) : type(Type::Text), content(content) {}

    static inline HtmlNodePtr ElementNodePtr(const QString &name, const CssProperties &attrs,
        const QVector<HtmlNodePtr> &children = {})
    { return std::make_shared<HtmlNode>(name, attrs, children); }

    static inline HtmlNodePtr TextNodePtr(const QString &content)
    { return std::make_shared<HtmlNode>(content); }
};

class HtmlRenderContext
{
public:
    CssProperties getStyleFor(HtmlNodePtr node) const;

    inline void insertStyle(const QString &styleText)
    { parseCssRules(styleText); }

    inline void insertMetadata(const QString &name, const QString &content)
    { m_metadata.insert(name, content); }

    void clear();

private:
    typedef QMap<QString, QString> HtmlMetadata;

    CssRules m_rules;
    HtmlMetadata m_metadata;

    // Parses an inline style string (e.g., "color: red; font-weight: bold;") into a dictionary.
    static CssProperties parseInlineString(const QString &styleStr);

    /*
     * Parses a simple CSS string and returns a dictionary mapping individual selectors
     * to their respective property dictionaries.
     *
     * Example:
     *     Input:
     *         "p { margin-top: 10px; color: red; } h1 { font-size: 20pt; }"
     *     Output:
     *         {
     *             "p": {"margin-top": "10px", "color": "red"},
     *             "h1": {"font-size": "20pt"}
     *         }
     *
     * Handles grouped selectors like:
     *     "p, li { margin-left: 0px; }"
     * correctly as:
     *     {
     *         "p": {"margin-left": "0px"},
     *         "li": {"margin-left": "0px"}
     *    }
     * 
     * It currently does not handle every legal edge case, for example semicolons inside quotes strings.
     */
    void parseCssRules(const QString &cssText);
};

class QTextList;

/*
 * Imports an HTML string to a QTextDocument from a focused subset of HTML.
 * The main purpose of this importer is to losslessly cut, copy and paste
 * between QTextDocuments created with TextEditor.
 * Therefore it doesn't parse a lot of tags and styles.
 * Should be extended whenever suitable.
 */
class HtmlImporter
{
public:
    explicit HtmlImporter(QTextDocument *document);

    void import(QString html);

private:
    QTextDocument *m_document;
    QString m_input;
    HtmlRenderContext m_context;

    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFmt;
    QTextCharFormat m_charFmt;

    bool m_atBeginning;
    bool m_newParagraph;
    bool m_newListItem;

    int m_indent;
    int m_nestedUlTags;
    QTextList *m_currentList;

    // Render functions
    void renderNode(const HtmlNodePtr &node);
    void insertBlock();
    void finalizeBlock();

    void applyBlockFormatStyle(const CssProperties &style);

    // Tokenizer
    QVector<HtmlToken> tokenize() const;

    // Parser
    QVector<HtmlNodePtr> parse(const QVector<HtmlToken> &tokens) const;
    QVector<HtmlNodePtr> parseChildNodes(const QVector<HtmlToken> &tokens, int &pos,
        const QString &stopTag = {}) const;

    // Render context
    void extractContext(HtmlNodePtr headNode);

    /*
     * findNode recursively searches for a node with the given tag name in the AST.
     * 
     * Parameters:
     *     root (HtmlNodePtr): The root of the AST or a list of nodes.
     *     tagName (QString): The tag name to search for (case-insensitive).
     *
     * Returns:
     *     HtmlNode or None: The first node matching the tag name, or None if not found.
     */
    static HtmlNodePtr findNode(HtmlNodePtr root, const QString &tagName);

    // Search in a list rather than a single root element
    static HtmlNodePtr findNode(const QVector<HtmlNodePtr> &nodes, const QString &tagName);

    using TokenType = HtmlToken::Type;
    using NodeType = HtmlNode::Type;
};

#endif
