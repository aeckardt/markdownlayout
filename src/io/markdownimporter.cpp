#include "markdownimporter.h"

#include "3rdparty/md4c/md4c.h"
#include "textformat/blocktypes.h"
#include "textformat/constdefs.h"
#include "htmlstyle.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QStack>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFormat>
#include <QTextList>
#include <QTextListFormat>

using namespace Qt::StringLiterals;

class MarkdownRenderer
{
public:
    static QTextDocument *createDocument(const QByteArray &markdown, QObject *parent = nullptr);

    // Callbacks
    int cbEnterBlock(int blockType, void *detail);
    int cbLeaveBlock(int blockType, void *detail);
    int cbEnterSpan(int spanType, void *detail);
    int cbLeaveSpan(int spanType, void *detail);
    int cbText(int textType, const char *text, unsigned size);

private:
    explicit MarkdownRenderer(QTextCursor *cursor);

    // Render functions
    void insertBlock();
    void finalizeBlock();

    struct TextList {
        QTextListFormat fmt;
        QTextList *list = nullptr;
    };

    // Render state variables
    QTextCursor *m_cursor;
    QTextBlockFormat m_blockFmt;
    QStack<QTextCharFormat> m_charFmtStack;

    bool m_atBeginning;
    bool m_newParagraph;
    bool m_newListItem;
    QStack<TextList> m_listStack;
    int m_blockQuoteLevel;
};

static int _cbEnterBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    MarkdownRenderer *mdr = static_cast<MarkdownRenderer *>(userdata);
    return mdr->cbEnterBlock(int(type), detail);
}

static int _cbLeaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    MarkdownRenderer *mdr = static_cast<MarkdownRenderer *>(userdata);
    return mdr->cbLeaveBlock(int(type), detail);
}

static int _cbEnterSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    MarkdownRenderer *mdr = static_cast<MarkdownRenderer *>(userdata);
    return mdr->cbEnterSpan(int(type), detail);
}

static int _cbLeaveSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    MarkdownRenderer *mdr = static_cast<MarkdownRenderer *>(userdata);
    return mdr->cbLeaveSpan(int(type), detail);
}

static int _cbText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    MarkdownRenderer *mdr = static_cast<MarkdownRenderer *>(userdata);
    return mdr->cbText(int(type), text, size);
}

QTextDocument *MarkdownRenderer::createDocument(const QByteArray &markdown, QObject *parent)
{
    QTextDocument *document = new QTextDocument(parent);
    QTextCursor cursor(document);

    // Do not create an undo when creating the document from the input
    document->setUndoRedoEnabled(false);
    cursor.beginEditBlock();

    MarkdownRenderer mdr(&cursor);

    unsigned flags = 0;
    MD_PARSER callbacks = {
        0,       // abi_version
        flags,
        &_cbEnterBlock,
        &_cbLeaveBlock,
        &_cbEnterSpan,
        &_cbLeaveSpan,
        &_cbText,
        nullptr, // debug_log
        nullptr  // syntax
    };

    // Parse with MD4C / Render with callbacks
    md_parse(markdown.constData(), MD_SIZE(markdown.size()), &callbacks, &mdr);

    // Restore undoRedoEnabled
    cursor.endEditBlock();
    document->setUndoRedoEnabled(true);

    return document;
}

MarkdownRenderer::MarkdownRenderer(QTextCursor *cursor)
    : m_cursor(cursor),
      m_blockFmt(defaultBlockFormat()),
      m_atBeginning(true),
      m_newParagraph(false),
      m_newListItem(false),
      m_blockQuoteLevel(0)
{
    Q_ASSERT(m_cursor);
    m_charFmtStack.push(defaultCharFormat());
}

int MarkdownRenderer::cbEnterBlock(int blockType, void *detail)
{
    switch (blockType) {
    case MD_BLOCK_H: {
        MD_BLOCK_H_DETAIL *h_detail = static_cast<MD_BLOCK_H_DETAIL *>(detail);

        // Start heading in a new block
        insertBlock();

        // Set heading level property
        m_blockFmt.setHeadingLevel(h_detail->level);

        // Adjust block char format for new block
        QTextCharFormat headingCharFmt = headingFormatModifier(h_detail->level, QTextCharFormat());
        m_cursor->mergeBlockCharFormat(headingCharFmt);

        // Use heading char format for next fragment(s)
        QTextCharFormat newCharFmt(m_charFmtStack.top());
        newCharFmt.merge(headingCharFmt);
        m_charFmtStack.push(newCharFmt);

        break;
    }
    case MD_BLOCK_LI: {
        // Use new line for list item
        insertBlock();

        QTextList *currentList = m_listStack.top().list;
        if (!currentList) {
            // Setup list, if not yet available
            currentList = m_cursor->createList(m_listStack.top().fmt);
            m_listStack.top().list = currentList;
        }

        // Attach QTextList object to block format
        m_blockFmt.setObjectIndex(currentList->objectIndex());

        // Activate safety guard for not adding a newline with a paragraph directly after
        m_newListItem = true;

        break;
    }
    case MD_BLOCK_P:
        // Safety guard for not adding a new line directly after a list item
        if (!m_newListItem)
            insertBlock();
        else
            m_newListItem = false;
        m_newParagraph = true;
        break;
    case MD_BLOCK_HR:
        if (!m_newParagraph) {
            finalizeBlock();
            insertBlock();
        } else
            // # Don't add a new line if the paragraph just started
            // # Because a new line has already been added!
            m_newParagraph = false;

        // Set horizontal rule property for this block
        m_blockFmt.setProperty(QTextFormat::Property::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
        if (horizontalRulerColor().isValid())
            m_blockFmt.setProperty(QTextFormat::Property::BackgroundBrush, horizontalRulerColor());

        // Since HR is a self enclosing tag, it's safe to return
        finalizeBlock();
        return 0;
    case MD_BLOCK_UL: {
        MD_BLOCK_UL_DETAIL *ul_detail = static_cast<MD_BLOCK_UL_DETAIL *>(detail);

        static const QHash<MD_CHAR, QTextListFormat::Style> styles = {
            {'*', QTextListFormat::ListDisc},
            {'+', QTextListFormat::ListSquare},
            {'-', ExtendedListStyle::ListDash}
        };

        TextList list;
        list.fmt.setIndent(m_listStack.count() + 1);
        list.fmt.setStyle(styles.contains(ul_detail->mark)
                          ? styles[ul_detail->mark]
                          : QTextListFormat::ListCircle);
        m_listStack.push(list);

        break;
    }
    case MD_BLOCK_QUOTE:
        ++m_blockQuoteLevel;
        break;
    default:
        // Other cases are ignored so far
        ;
    }

    if (m_newParagraph && blockType != MD_BLOCK_P)
        // Remove guard for not adding a new line when a break follows directly after a paragraph
        m_newParagraph = false;
    if (m_newListItem && blockType != MD_BLOCK_LI)
        // Remove guard for not adding a new line when a paragraph follows directly after a list item
        m_newListItem = false;

    return 0;  // no error
}

int MarkdownRenderer::cbLeaveBlock(int blockType, void *detail)
{
    Q_UNUSED(detail)

    switch (blockType) {
    case MD_BLOCK_H:
        // Revert heading char format modifications
        m_charFmtStack.pop();
        finalizeBlock();
        m_blockFmt.setHeadingLevel(0);
        break;
    case MD_BLOCK_LI:
        finalizeBlock();
        break;
    case MD_BLOCK_P:
        if (!m_newListItem)
            finalizeBlock();
        m_newParagraph = false;
        break;
    case MD_BLOCK_UL:
        m_listStack.pop();
        break;
    case MD_BLOCK_QUOTE:
        finalizeBlock();
        --m_blockQuoteLevel;
        break;
    default:
        // Nothing to do in other cases
        ;
    }

    return 0;  // no error
}

int MarkdownRenderer::cbEnterSpan(int spanType, void *detail)
{
    switch (spanType) {
    case MD_SPAN_STRONG: {
        QTextCharFormat boldFmt(m_charFmtStack.top());
        boldFmt.setFontWeight(StrongFontWeight);
        m_charFmtStack.push(boldFmt);
        break;
    }
    case MD_SPAN_EM: {
        QTextCharFormat italicFmt(m_charFmtStack.top());
        italicFmt.setFontItalic(true);
        m_charFmtStack.push(italicFmt);
        break;
    }
    case MD_SPAN_U: {
        QTextCharFormat underlineFmt(m_charFmtStack.top());
        underlineFmt.setFontUnderline(true);
        m_charFmtStack.push(underlineFmt);
        break;
    }
    case MD_SPAN_A: {
        MD_SPAN_A_DETAIL *a_detail = static_cast<MD_SPAN_A_DETAIL *>(detail);

        QTextCharFormat linkFmt(m_charFmtStack.top());
        linkFmt.setAnchor(true);
        if (a_detail->href.size > 0) {
            const QString href = QUrl::fromPercentEncoding(
                        QByteArray(a_detail->href.text, a_detail->href.size));
            linkFmt.setAnchorHref(href);
        }
        linkFmt.setForeground(linkColor());
        m_charFmtStack.push(linkFmt);

        break;
    }
    default:
        // Images, inline code, strikethrough etc. are currently not styled.
        // Their child text still arrives through cbText().
        ;
    }

    return 0;  // no error
}

int MarkdownRenderer::cbLeaveSpan(int spanType, void *detail)
{
    Q_UNUSED(detail);

    switch (spanType) {
    case MD_SPAN_STRONG:
    case MD_SPAN_EM:
    case MD_SPAN_U:
    case MD_SPAN_A:
        m_charFmtStack.pop();
        break;
    default:
        ;
    }

    return 0;  // no error
}

int MarkdownRenderer::cbText(int textType, const char *text, unsigned int size)
{
    switch (textType) {
    case MD_TEXT_NORMAL:
        // Insert text with the current char format
        m_cursor->insertText(QString::fromUtf8(text, size), m_charFmtStack.top());

        // Remove newline guards because text has been added
        m_newParagraph = false;
        m_newListItem = false;

        break;
    case MD_TEXT_BR:
        if (!m_newParagraph) {
            finalizeBlock();
            insertBlock();
        } else
            // Don't add a new line if the paragraph just started
            // Because a new line has already been added!
            m_newParagraph = false;
        break;
    default:
        ;
    }

    return 0;  // no error
}

void MarkdownRenderer::insertBlock()
{
    if (m_atBeginning) {
        m_atBeginning = false;
        return;
    }

    m_cursor->insertBlock();
    m_blockFmt = defaultBlockFormat();
    m_charFmtStack.clear();
    m_charFmtStack.push(defaultCharFormat());
    m_cursor->setCharFormat(m_charFmtStack.top());
}

void MarkdownRenderer::finalizeBlock()
{
    // Set block format for current block before adding new block
    if (m_blockQuoteLevel > 0)
        m_blockFmt.setProperty(QTextFormat::BlockQuoteLevel, m_blockQuoteLevel);
    m_cursor->setBlockFormat(m_blockFmt);

    // Use current QTextCharFormat to set block charformat for the next line
    Q_ASSERT(m_charFmtStack.size() == 1);
    m_cursor->setCharFormat(m_charFmtStack.top());
}

QTextDocument *documentFromMarkdown(const QByteArray &markdown, QObject *parent)
{
    return MarkdownRenderer::createDocument(markdown, parent);
}
