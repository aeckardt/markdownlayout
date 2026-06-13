#ifndef MARKDOWNINLINEPARSER_H
#define MARKDOWNINLINEPARSER_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QSharedPointer>

struct InlineNode;
typedef std::shared_ptr<InlineNode> InlineNodePtr;

struct InlineNode {
    enum class Type {
        Invalid,
        Container,
        Strong,
        Emph,
        InlineLink,
        Image,
        HtmlTag,
        Text
    };

    inline InlineNode() : type(Type::Invalid) {}
    inline InlineNode(Type type, const QVector<InlineNodePtr> &children = {})
        : type(type), children(children) {}
    inline InlineNode(Type type, const QString &content, const QMap<QString, QVariant> &attrs = {})
        : type(type), content(content), attrs(attrs) {}
    InlineNode(const InlineNode &) = delete;

    void clear();
    inline bool isValid() const { return type != Type::Invalid; }
    operator bool() const { return type != Type::Invalid; }

    Type type;
    QString content;  // Contains text or HTML tag name
    QMap<QString, QVariant> attrs;
    QVector<InlineNodePtr> children;
};

/*
 * A "run" of delimiters.
 * E.g. *** is a sequence of delimiter characters treated as one unit.
 *     An * is a delimiter.
 *     An ** is a delimiter run of length 2.
 *     An *** is a delimiter run of length 3.
 * The content field contains the literal delimiter text.
 */
struct DelimiterRun {
    QString content;  // e.g. '*', '**', '***', '!['
};

struct InlineHtmlTag {
    QString content;  // Original input string for the tag
    QString tag;      // "ins", "span", ...
    QMap<QString, QVariant> attrs;

    inline InlineHtmlTag() {}
    inline InlineHtmlTag(QString content, QString tag, QMap<QString, QVariant> attrs = {})
        : content(content), tag(tag), attrs(attrs) {}
};

/*
 * Every open instance of ScopeMarker contains structure in form of a
 * floating node. This is wrapped by a closing ScopeMarker and then integrated
 * into the tree. Alternatively, the marker will be disregarded as text.
 */
class ScopeMarker {
public:
    enum class Type : int {
        Invalid  = -1,
        // The integer values are also their priority
        // Lower integer = higher binding priority when resolving the stack
        HtmlTag  = 0, // "<ins>", "<span>", ...
        Bracket  = 1, // "![", "[" or "]"
        Asterisk = 2  // "*", "**", "***", ...
    };

    inline ScopeMarker() : m_type(Type::Invalid), m_data(nullptr), m_node(nullptr) {}
    ScopeMarker(Type type, const DelimiterRun &dr, bool canOpen, bool canClose);
    ScopeMarker(Type type, const InlineHtmlTag &iht, bool canOpen, bool canClose);
    ScopeMarker(const ScopeMarker &) = delete;
    ~ScopeMarker();

    inline bool isValid() const { return m_type != Type::Invalid; }
    inline operator bool() const { return m_type != Type::Invalid; }

    inline Type type() const { return m_type; }

    const DelimiterRun &delimiterRun() const;
    const InlineHtmlTag &htmlTag() const;

    QString content() const;
    void resetContent(const QString &content);

    bool canOpen() const { return m_canOpen; }
    bool canClose() const { return m_canClose; }

    void createNode(InlineNode::Type type, const QVector<InlineNodePtr> &children = {});
    void createNode(InlineNode::Type type, const QString &content, const QMap<QString, QVariant> &attrs = {});
    InlineNodePtr node() const { return m_node; }
    void clearNode();

private:
    Type m_type;
    void *m_data;
    bool m_canOpen;
    bool m_canClose;
    InlineNodePtr m_node;
};

typedef std::shared_ptr<ScopeMarker> ScopeMarkerPtr;

class MarkdownInlineParser {
public:
    explicit MarkdownInlineParser(QString input = {});

    inline const InlineNodePtr astRoot() const { return m_astRoot; }

private:
    void parse();
    ScopeMarkerPtr findOpeningMarker(ScopeMarkerPtr marker);

    // Emphasis marker parsing
    void consumeMatch(ScopeMarkerPtr leftMarker, ScopeMarkerPtr rightMarker);
    void finalizeEmphasisMarker(ScopeMarkerPtr marker);

    // HTML tag parsing
    ScopeMarkerPtr tryParseHtmlTag();
    QString readIdentifier(int &fwdPos) const;
    void skipWhitespaces(int &fwdPos) const;
    QString readAttributeValue(int &fwdPos) const;

    void pushScopeMarker(ScopeMarkerPtr marker);
    ScopeMarkerPtr popScopeMarker();
    void integrateNode(InlineNodePtr node);
    void integrateMarkerAsText(ScopeMarkerPtr marker);
    void flushText();

    const QString m_input;
    InlineNodePtr m_astRoot;
    InlineNodePtr m_currentParent;
    QVector<ScopeMarkerPtr> m_openScopeStack;
    int m_pos = 0;
    int m_length = 0;
    QString m_text;

    using NodeType = InlineNode::Type;
    using MarkerType = ScopeMarker::Type;
};

#endif
