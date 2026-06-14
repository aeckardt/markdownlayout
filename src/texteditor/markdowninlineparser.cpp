#include "texteditor/markdowninlineparser.h"

#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrl>

#include <algorithm>
#include <optional>

bool isPunctuation(const std::optional<QChar> &ch)
{
    if (!ch.has_value())
        return false;
    switch (ch->category()) {
    case QChar::Punctuation_Connector:
    case QChar::Punctuation_Dash:
    case QChar::Punctuation_Open:
    case QChar::Punctuation_Close:
    case QChar::Punctuation_InitialQuote:
    case QChar::Punctuation_FinalQuote:
    case QChar::Punctuation_Other:
        return true;
    default:
        return false;
    }
}

void InlineNode::clear()
{
    content.clear();
    attrs.clear();
    children.clear();
    type = Type::Invalid;
}

ScopeMarker::ScopeMarker(Type type, const DelimiterRun &dr, bool canOpen, bool canClose)
    : m_type(type),
      m_data(new DelimiterRun(dr)),
      m_canOpen(canOpen),
      m_canClose(canClose),
      m_node(nullptr)
{
    Q_ASSERT(type == Type::Bracket || type == Type::Asterisk);
}

ScopeMarker::ScopeMarker(const InlineHtmlTag &iht, bool canOpen, bool canClose)
    : m_type(Type::HtmlTag),
      m_data(new InlineHtmlTag(iht)),
      m_canOpen(canOpen),
      m_canClose(canClose),
      m_node(nullptr)
{
}

ScopeMarker::~ScopeMarker()
{
    switch (m_type) {
    case Type::HtmlTag:
        delete (InlineHtmlTag *)m_data;
        break;
    case Type::Bracket:
    case Type::Asterisk:
        delete (DelimiterRun *)m_data;
        break;
    default:
        ;
    }
}

const DelimiterRun &ScopeMarker::delimiterRun() const
{
    Q_ASSERT(m_type == Type::Bracket || m_type == Type::Asterisk);
    return *static_cast<const DelimiterRun *>(m_data);
}

const InlineHtmlTag &ScopeMarker::htmlTag() const
{
    Q_ASSERT(m_type == Type::HtmlTag);
    return *static_cast<const InlineHtmlTag *>(m_data);
}

QString ScopeMarker::content() const
{
    switch (m_type) {
    case Type::HtmlTag:
        return static_cast<InlineHtmlTag *>(m_data)->content;
    case Type::Bracket:
    case Type::Asterisk:
        return static_cast<DelimiterRun *>(m_data)->content;
    default:
        return {};
    }
}

void ScopeMarker::resetContent(const QString &content)
{
    switch (m_type) {
    case Type::HtmlTag:
        static_cast<InlineHtmlTag *>(m_data)->content = content;
        break;
    case Type::Bracket:
    case Type::Asterisk:
        static_cast<DelimiterRun *>(m_data)->content = content;
        break;
    default:
        ;
    }
}

void ScopeMarker::createNode(InlineNode::Type type, const QVector<InlineNodePtr> &children)
{
    m_node = std::make_shared<InlineNode>(type, children);
}

void ScopeMarker::createNode(InlineNode::Type type, const QString &content, const QMap<QString, QVariant> &attrs)
{
    m_node = std::make_shared<InlineNode>(type, content, attrs);
}

void ScopeMarker::clearNode()
{
    m_node.reset();
}

MarkdownInlineParser::MarkdownInlineParser(QString input)
    : m_input(input),
      m_astRoot(new InlineNode(NodeType::Container)),
      m_currentParent(nullptr)
{
    if (!m_input.isEmpty())
        parse();
}

void MarkdownInlineParser::parse()
{
    // Initialize data
    m_astRoot->clear();
    m_astRoot->type = NodeType::Container;

    m_currentParent = m_astRoot;
    m_openScopeStack.clear();

    m_pos = 0;
    m_length = m_input.size();
    m_text.clear();

    // Iterate through input string
    while (m_pos < m_length) {
        QChar ch = m_input.at(m_pos);
        switch (ch.unicode()) {
        case u'*': {
            flushText();

            // Count the asterisks
            int count = 1;
            while (m_pos + count < m_length && m_input.at(m_pos + count) == QLatin1Char('*'))
                ++count;

            // Character just before the run
            std::optional<QChar> prevChar;
            if (m_pos > 0)
                prevChar = m_input.at(m_pos - 1);

            // Character just after the run
            std::optional<QChar> nextChar;
            if (m_pos + count < m_length)
                nextChar = m_input.at(m_pos + count);

            const bool prevIsWs = prevChar ? prevChar->isSpace() : true;
            const bool prevIsP = isPunctuation(prevChar);
            const bool prevIsTc = prevChar && *prevChar == QLatin1Char('>');
            const bool nextIsWs = nextChar ? nextChar->isSpace() : true;
            const bool nextIsP = isPunctuation(nextChar);
            const bool nextIsTo = nextChar && *nextChar == QLatin1Char('<');

            // A run is left-flanking if:
            //  1) next_char is not whitespace, AND
            //  2) (next_char is not punctuation OR prev_char is whitespace or punctuation)
            const bool leftFlanking = !nextIsWs && (!nextIsP || prevIsWs || prevIsP || prevIsTc);

            // A run is right-flanking if:
            //  1) prev_char is not whitespace, AND
            //  2) (prev_char is not punctuation OR next_char is whitespace or punctuation)
            const bool rightFlanking = !prevIsWs && (!prevIsP || nextIsWs || nextIsP || nextIsTo);

            ScopeMarkerPtr marker = ScopeMarker::makeAsterisk(
                        {QString(count, QLatin1Char('*'))},
                        leftFlanking, rightFlanking);

            if (rightFlanking) {
                // Get matching marker node from stack
                ScopeMarkerPtr openMarker = findOpeningMarker(marker);
                if (openMarker)
                    // Resolve the two markers (left and right side may have different length)
                    consumeMatch(openMarker, marker);
                else if (leftFlanking)
                    pushScopeMarker(marker);
                else
                    // The marker could not be matched
                    // Will be added as text...
                    integrateMarkerAsText(marker);
            } else if (leftFlanking)
                pushScopeMarker(marker);
            else
                integrateMarkerAsText(marker);

            // Advance past the asterisks
            m_pos += count;
            break;
        }
        case u'!': {
            if (m_pos + 1 < m_length && m_input.at(m_pos + 1) == QLatin1Char('[')) {
                // '![' found at pos => push ScopeMarker to stack
                flushText();
                ScopeMarkerPtr marker = ScopeMarker::makeBracket({QStringLiteral("![")}, true, false);
                pushScopeMarker(marker);
                m_pos += 2;
            } else {
                // Disregard as text
                m_text += ch;
                ++m_pos;
            }
            break;
        }
        case u'[': {
            flushText();
            ScopeMarkerPtr marker = ScopeMarker::makeBracket({QStringLiteral("[")}, true, false);
            pushScopeMarker(marker);
            ++m_pos;
            break;
        }
        case u']': {
            flushText();
            ScopeMarkerPtr marker = ScopeMarker::makeBracket({QStringLiteral("]")}, false, true);
            ++m_pos;

            // Get matching marker node from stack
            auto openMarker = findOpeningMarker(marker);
            if (!openMarker) {
                integrateMarkerAsText(marker);
                continue;
            }

            // Check if next char is opening parenthesis
            if (m_pos >= m_length || m_input.at(m_pos) != QLatin1Char('(')) {
                integrateMarkerAsText(openMarker);
                integrateMarkerAsText(marker);
                continue;
            }

            // Look ahead to find closing parenthesis
            int fwdpos = m_pos + 1;
            QString resPath;
            while (fwdpos < m_length) {
                ch = m_input.at(fwdpos);
                if (ch == QLatin1Char('(') || ch == QLatin1Char(')'))
                    break;
                resPath += ch;
                ++fwdpos;
            }
            resPath = resPath.trimmed();
            static const QRegularExpression whitespaceRe(QStringLiteral("\\s"));
            if (ch == QLatin1Char('(') || whitespaceRe.match(resPath).hasMatch()) {
                // Condition for valid link / image syntax violated!
                integrateMarkerAsText(openMarker);
                integrateMarkerAsText(marker);
                continue;
            }

            resPath = QUrl::fromPercentEncoding(resPath.toUtf8());

            // Valid link / image syntax found!
            InlineNodePtr node = openMarker->node();
            if (openMarker->delimiterRun().content == QStringLiteral("![")) {
                node->type = NodeType::Image;
                node->attrs.insert(QStringLiteral("src"), resPath);
            } else {
                node->type = NodeType::InlineLink;
                node->attrs.insert(QStringLiteral("href"), resPath);
            }

            // Integrate node into tree
            integrateNode(node);

            // Advance to after the closing parenthesis
            m_pos = fwdpos + 1;
            break;
        }
        case u'<': {
            flushText();

            // Parse HTML separately
            ScopeMarkerPtr marker = tryParseHtmlTag();
            if (!marker) {
                m_text += ch;
                ++m_pos;
                continue;
            }

            if (marker->canOpen())
                pushScopeMarker(marker);
            else if (marker->canClose()) {
                ScopeMarkerPtr openMarker = findOpeningMarker(marker);
                if (!openMarker) {
                    integrateMarkerAsText(marker);
                    continue;
                }
                openMarker->createNode(NodeType::HtmlTag,
                                       openMarker->htmlTag().tag,
                                       openMarker->htmlTag().attrs);
                integrateNode(openMarker->node());
            } else {
                marker->createNode(NodeType::HtmlTag,
                                   marker->htmlTag().tag,
                                   marker->htmlTag().attrs);
                integrateNode(marker->node());
            }
            continue;
        }
        default:
            m_text += ch;
            ++m_pos;
        }
    }

    flushText();
    while (!m_openScopeStack.isEmpty()) {
        ScopeMarkerPtr marker = popScopeMarker();
        integrateMarkerAsText(marker);
    }
}

ScopeMarkerPtr MarkdownInlineParser::findOpeningMarker(ScopeMarkerPtr marker)
{
    int index = m_openScopeStack.size() - 1;
    while (index >= 0) {
        const ScopeMarkerPtr &openMarker = m_openScopeStack[index];
        if (openMarker->type() == marker->type()) {
            if (marker->type() == MarkerType::HtmlTag) {
                const QString &openTag = openMarker->htmlTag().tag;
                const QString &closeTag = marker->htmlTag().tag;
                if (openTag == closeTag)
                    break;
            } else
                break;
        } else if (openMarker->type() < marker->type())
            // The open scope in the stack has higher priority
            // Therefore, no match is possible
            return {};
        --index;
    }
    if (index < 0)
        return {};

    while (true) {
        ScopeMarkerPtr poppedMarker = popScopeMarker();
        if (poppedMarker->type() == marker->type()) {
            if (marker->type() == MarkerType::HtmlTag) {
                const QString &poppedTag = poppedMarker->htmlTag().tag;
                const QString &closeTag = marker->htmlTag().tag;
                if (poppedTag == closeTag)
                    return poppedMarker;
            } else
                return poppedMarker;
        }
        // Interpret un-matched markers as text
        integrateMarkerAsText(poppedMarker);
    }
}

void MarkdownInlineParser::consumeMatch(ScopeMarkerPtr leftMarker, ScopeMarkerPtr rightMarker)
{
    // Marker types are assumed to be asterisk, when calling this function
    const int leftLength = leftMarker->delimiterRun().content.size();
    const int rightLength = rightMarker->delimiterRun().content.size();

    if (leftLength == rightLength) {
        // Append matched marker node to higher node
        finalizeEmphasisMarker(leftMarker);
        integrateNode(leftMarker->node());
    } else if (leftLength > rightLength) {
        // Split node: Move the structure from the left side to the right side
        // and then add the right side marker as child to the left side

        // First, create a node for the right side marker and finalize it
        rightMarker->createNode(NodeType::Container, leftMarker->node()->children);
        finalizeEmphasisMarker(rightMarker);

        // Second, make the right side node a child of the left side node
        leftMarker->node()->children = {rightMarker->node()};

        // Reduce open number of asterisks on left side
        leftMarker->resetContent(QString(leftLength - rightLength, QLatin1Char('*')));

        // Put the left marker back on the stack
        pushScopeMarker(leftMarker);
    } else /* (left_length < right_length) */ {
        // Integrate matched marker node to the tree
        finalizeEmphasisMarker(leftMarker);
        integrateNode(leftMarker->node());

        // Update marker to match left_length chars
        rightMarker->resetContent(QString(rightLength - leftLength, QLatin1Char('*')));

        // Try to match asterisks to the left with the altered stack
        ScopeMarkerPtr openMarker = findOpeningMarker(rightMarker);
        if (!openMarker) {
            pushScopeMarker(rightMarker);
            return;
        }
        consumeMatch(openMarker, rightMarker);
    }
}

void MarkdownInlineParser::finalizeEmphasisMarker(ScopeMarkerPtr marker)
{
    const int count = marker->content().size();
    InlineNodePtr node = marker->node();
    if (count % 2 == 1) {
        if (count > 1) {
            InlineNodePtr subNode = std::make_shared<InlineNode>(NodeType::Emph, node->children);
            node->type = NodeType::Strong;
            node->children = {subNode};
        } else
            node->type = NodeType::Emph;
    } else
        node->type = NodeType::Strong;
}

ScopeMarkerPtr MarkdownInlineParser::tryParseHtmlTag()
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
    const QString tagName = readIdentifier(fwdPos);
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
            ScopeMarkerPtr marker = ScopeMarker::makeHtmlTag(
                        {m_input.mid(m_pos, fwdPos - m_pos + 1), tagName},
                        false, true);
            m_pos = fwdPos + 1;
            return marker;
        }
        // Condition for valid closing tag violated
        // '>' expected, but not found
        return {};
    }

    // Parse attributes or end of tag
    QMap<QString, QVariant> attrs;
    while (fwdPos < m_length) {
        QChar ch = m_input.at(fwdPos);
        if (ch.isSpace())
            ++fwdPos;
        else if (ch.isLetter()) {
            // Parse attribute
            // Read attribute name
            const QString attrName = readIdentifier(fwdPos);
            if (attrName.isEmpty())
                return {};

            // Consume whitespaces
            skipWhitespaces(fwdPos);
            if (fwdPos >= m_length)
                return {};

            // Check for equality sign
            QVariant attrValue;
            if (m_input.at(fwdPos) == QLatin1Char('=')) {
                ++fwdPos;

                // Consume whitespaces
                skipWhitespaces(fwdPos);
                if (fwdPos >= m_length)
                    return {};

                QString value;
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

                attrValue.setValue(value);
            } else
                attrValue.setValue(true);
            attrs.insert(attrName, attrValue);
        } else if (ch == QLatin1Char('/')) {
            // Self closing tag
            if (fwdPos + 1 < m_length && m_input.at(fwdPos + 1) == QLatin1Char('>')) {
                // Valid self closing tag found!
                ScopeMarkerPtr marker = ScopeMarker::makeHtmlTag(
                            {m_input.mid(m_pos, fwdPos - m_pos + 2), tagName, attrs},
                            false, false);
                m_pos = fwdPos + 2;
                return marker;
            }
            return {};
        } else if (ch == QLatin1Char('>')) {
            // Closing tag found
            ScopeMarkerPtr marker = ScopeMarker::makeHtmlTag(
                        {m_input.mid(m_pos, fwdPos - m_pos + 1), tagName, attrs},
                        true, false);
            m_pos = fwdPos + 1;
            return marker;
        } else
            // Other characters are not allowed here:
            return {};
    }

    return {};
}

QString MarkdownInlineParser::readIdentifier(int &fwdPos) const
{
    // Read name
    QString identifier = m_input.at(fwdPos);

    // The identifier needs to start with an alphabetic character
    if (!identifier.at(0).isLetter())
        return {};
    ++fwdPos;
    while (fwdPos < m_length && (m_input.at(fwdPos).isLetterOrNumber() ||
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

void MarkdownInlineParser::skipWhitespaces(int &fwdPos) const
{
    while (fwdPos < m_length && m_input.at(fwdPos).isSpace())
        ++fwdPos;
}

QString MarkdownInlineParser::readAttributeValue(int &fwdPos) const
{
    QString value;
    while (fwdPos < m_length) {
        const QChar ch = m_input.at(fwdPos);
        if (ch.isLetterOrNumber() || QStringLiteral("-_.:&;,").contains(ch)) {
            value += ch;
            ++fwdPos;
        } else
            return value;
    }
    return {};
}

void MarkdownInlineParser::pushScopeMarker(ScopeMarkerPtr marker)
{
    if (!marker->node()) {
        // Create a node for an opening marker
        // It's temporarily a floating node which can have children
        // Later it will either
        // - be integrated into the tree or
        // - it will be flattened and the marker will be treated as text
        marker->createNode(NodeType::Container);
    }
    m_currentParent = marker->node();

    // Push marker to the stack
    m_openScopeStack.append(std::move(marker));
}

ScopeMarkerPtr MarkdownInlineParser::popScopeMarker()
{
    ScopeMarkerPtr popped = m_openScopeStack.takeLast();

    // Update current parent
    if (!m_openScopeStack.isEmpty())
        m_currentParent = m_openScopeStack.last()->node();
    else
        m_currentParent = m_astRoot;

    return popped;
}

void MarkdownInlineParser::integrateNode(InlineNodePtr node)
{
    // Append the text node to the current parent node
    m_currentParent->children.append(node);
}

void MarkdownInlineParser::integrateMarkerAsText(ScopeMarkerPtr marker)
{
    // Append text node with marker characters to the tree
    integrateNode(std::make_shared<InlineNode>(NodeType::Text, marker->content()));

    if (marker->node()) {
        // Flatten the structure by moving all the children from the floating
        // marker node to the current parent node
        InlineNodePtr node = m_currentParent;
        node->children += std::move(marker->node()->children);
        marker->node()->children.clear();

        // The inline node from parsing is not needed anymore
        marker->clearNode();
    }
}

void MarkdownInlineParser::flushText()
{
    if (m_text.isEmpty())
        return;

    // Append new text node to the tree
    integrateNode(std::make_shared<InlineNode>(NodeType::Text, m_text));

    // Clear text
    m_text.clear();
}
