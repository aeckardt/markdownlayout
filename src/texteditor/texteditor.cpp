#include "texteditor.h"
#include "texteditor_p.h"

#include "htmlexporter.h"
#include "htmlimporter.h"
#include "markdownexporter.h"
#include "markdownimporter.h"
#include "linkeditordialog.h"
#include "texteditorstyle.h"
#include "texteditorstyle_p.h"

#include <QAction>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFormat>
#include <QTextList>
#include <QUrl>

#include <algorithm>

using namespace TextEditorStyle;

TextEditor::TextEditor(QWidget *parent)
    : QTextEdit(parent),
      m_cursorX(-1),
      m_keepCursorX(false),
      m_ctrlPressed(false)
{
    // Connect signals
    connect(this, &QTextEdit::currentCharFormatChanged,
            this, &TextEditor::onCurrentCharFormatChanged);
    connect(this, &QTextEdit::cursorPositionChanged,
            this, &TextEditor::onCursorPositionChanged);
    connect(this, &QTextEdit::selectionChanged,
            this, &TextEditor::onSelectionChanged);

    // Setup actions and menus
    setupActions();
    setupContextMenu();

    // Change viewport margin, if set
#ifdef VIEWPORT_MARGIN
    setViewportMargins(VIEWPORT_MARGIN, VIEWPORT_MARGIN, VIEWPORT_MARGIN, VIEWPORT_MARGIN);
    setStyleSheet(QStringLiteral("QTextEdit { background: white; }"));
#endif
}

void TextEditor::setDocument(QTextDocument *document)
{
    Q_ASSERT(document);

#ifdef DOCUMENT_INDENT_WIDTH
    // Change tab indent width (which me thinks, looks better than such a wide tab)
    document->setIndentWidth(DOCUMENT_INDENT_WIDTH);
#endif

    // Assign the imported document to the editor
    QTextEdit::setDocument(document);

    // Update undo, redo actions (the undoAvailable signal might not have been emitted)
    m_undoAction->setEnabled(document->isUndoAvailable());
    m_redoAction->setEnabled(document->isRedoAvailable());

    // Move cursor to the start
    moveCursor(QTextCursor::Start);
    ensureCursorVisible();
}

void TextEditor::setupActions()
{
    m_undoAction = new QAction(tr("Undo"), this);
    m_undoAction->setEnabled(false);
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(this, &QTextEdit::undoAvailable, m_undoAction, &QAction::setEnabled);
    connect(m_undoAction, &QAction::triggered, this, &QTextEdit::undo);

    m_redoAction = new QAction(tr("Redo"), this);
    m_redoAction->setEnabled(false);
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(this, &QTextEdit::redoAvailable, m_redoAction, &QAction::setEnabled);
    connect(m_redoAction, &QAction::triggered, this, &QTextEdit::redo);

    m_cutAction = new QAction(tr("Cut"), this);
    m_cutAction->setEnabled(false);
    connect(m_cutAction, &QAction::triggered, this, &QTextEdit::cut);

    m_copyAction = new QAction(tr("Copy"), this);
    m_copyAction->setEnabled(false);
    connect(m_copyAction, &QAction::triggered, this, &QTextEdit::copy);

    m_pasteAction = new QAction(tr("Paste"), this);
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &TextEditor::updatePasteAction);
    connect(m_pasteAction, &QAction::triggered, this, &QTextEdit::paste);
    updatePasteAction();

    m_removeAction = new QAction(tr("Remove"), this);
    m_removeAction->setEnabled(false);
    connect(m_removeAction, &QAction::triggered, this, [this] { textCursor().removeSelectedText(); });

    m_boldAction = new QAction(tr("Bold"), this);
    m_boldAction->setCheckable(true);
    m_boldAction->setShortcut(QKeySequence::Bold);
    connect(m_boldAction, &QAction::triggered, this, &TextEditor::updateBold);
    addAction(m_boldAction);

    m_italicAction = new QAction(tr("Italic"), this);
    m_italicAction->setCheckable(true);
    m_italicAction->setShortcut(QKeySequence::Italic);
    connect(m_italicAction, &QAction::triggered, this, &TextEditor::updateItalic);
    addAction(m_italicAction);

    m_underlineAction = new QAction(tr("Underline"), this);
    m_underlineAction->setCheckable(true);
    m_underlineAction->setShortcut(QKeySequence::Underline);
    connect(m_underlineAction, &QAction::triggered, this, &TextEditor::updateUnderline);
    addAction(m_underlineAction);

    m_textsizePlusAction = new QAction(tr("Increase font size"), this);
    m_textsizePlusAction->setShortcut(QKeySequence("Ctrl++"));
    connect(m_textsizePlusAction, &QAction::triggered, this, &TextEditor::textsizePlus);
    addAction(m_textsizePlusAction);

    m_textsizeMinusAction = new QAction(tr("Decrease font size"), this);
    m_textsizeMinusAction->setShortcut(QKeySequence("Ctrl+-"));
    connect(m_textsizeMinusAction, &QAction::triggered, this, &TextEditor::textsizeMinus);
    addAction(m_textsizeMinusAction);

    m_lessIndentAction = new QAction(tr("Decrease list indent"), this);
    m_lessIndentAction->setShortcut(QKeySequence("Shift+Tab"));
    connect(m_lessIndentAction, &QAction::triggered, this, &TextEditor::unindentSelection);

    m_moreIndentAction = new QAction(tr("Increase list indent"), this);
    m_moreIndentAction->setShortcut(QKeySequence("Tab"));
    connect(m_moreIndentAction, &QAction::triggered, this, &TextEditor::indentSelection);

    m_editLinkAction = new QAction(tr("Edit link"), this);
    m_editLinkAction->setEnabled(false);
    connect(m_editLinkAction, &QAction::triggered, this, &TextEditor::editHyperlink);

    m_insertEmojiAction = new QAction(tr("Insert emoji..."), this);
    m_insertEmojiAction->setShortcut(QKeySequence("Ctrl+E"));
    connect(m_insertEmojiAction, &QAction::triggered, this, &TextEditor::insertEmoji);
    addAction(m_insertEmojiAction);
}

void TextEditor::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_contextMenu->addAction(m_editLinkAction);
    m_contextMenuSeparator = new QAction(this);
    m_contextMenuSeparator->setSeparator(true);
    m_contextMenu->addAction(m_contextMenuSeparator);

    m_contextMenu->addAction(m_undoAction);
    m_contextMenu->addAction(m_redoAction);
    m_contextMenu->addSeparator();

    m_contextMenu->addAction(m_cutAction);
    m_contextMenu->addAction(m_copyAction);
    m_contextMenu->addAction(m_pasteAction);
    m_contextMenu->addAction(m_removeAction);
}

void TextEditor::onCurrentCharFormatChanged(const QTextCharFormat &format)
{
    emit fontChanged(format.font());

    m_boldAction->setChecked(isMarkdownStrong(format));
    m_italicAction->setChecked(format.fontItalic());
    m_underlineAction->setChecked(format.fontUnderline());
}

void TextEditor::onCursorPositionChanged()
{
    emit blockFormatChanged();

    const QTextCursor cursor = textCursor();

    if (!m_keepCursorX)
        updateCursorX(cursor);
    else
        m_keepCursorX = false;

    if (cursor.charFormat().isAnchor() && !cursor.hasSelection()) {
        HyperlinkPtr linkRef = getLinkUnderCursor(cursor);
        if (!linkRef) {
            // Change current char format at the beginning or end of a link
            // Thus the user can continue to write normal text, i.e. not with an anchor
            QTextCharFormat charFmt;
            charFmt.setAnchor(false);
            charFmt.setAnchorHref({});
            charFmt.setForeground(QColor("black"));
            mergeCurrentCharFormat(charFmt);
        }
    }
}

void TextEditor::onSelectionChanged()
{
    const bool hasSelection = textCursor().hasSelection();

    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_removeAction->setEnabled(hasSelection);
}

void TextEditor::updateBold()
{
    if (m_boldAction->isChecked()) {
        QTextCharFormat charFmt;
        charFmt.setFontWeight(StrongFontWeight);
        mergeFormatOnSelection(charFmt);
    } else
        // Since headings and paragraphs have different default weights
        // Clearing bold/strong requires a little more work
        clearStrongOnSelection();
}

void TextEditor::updateItalic()
{
    QTextCharFormat charFmt;
    charFmt.setFontItalic(m_italicAction->isChecked());
    mergeFormatOnSelection(charFmt);
}

void TextEditor::updateUnderline()
{
    QTextCharFormat charFmt;
    charFmt.setFontUnderline(m_underlineAction->isChecked());
    mergeFormatOnSelection(charFmt);
}

void TextEditor::setTextSize(const QString &sizeText)
{
    if (textCursor().blockFormat().headingLevel() > 0)
        return;

    bool ok = false;
    const double pointSize = sizeText.toDouble(&ok);
    if (!ok || pointSize <= 0)
        return;

    QTextCharFormat charFmt;
    charFmt.setFontPointSize(pointSize);
    mergeFormatOnSelection(charFmt);
}

void TextEditor::textsizePlus()
{
    QTextCursor cursor = textCursor();
    int headingLevel = cursor.blockFormat().headingLevel();
    if (headingLevel > 0)
        return;

    int pointSize = cursor.charFormat().font().pointSize();

    if (pointSize < 72) {
        QTextCharFormat charFmt;
        charFmt.setFontPointSize(qreal(pointSize + 1));
        mergeFormatOnSelection(charFmt);
    }
}

void TextEditor::textsizeMinus()
{
    QTextCursor cursor = textCursor();
    int headingLevel = cursor.blockFormat().headingLevel();
    if (headingLevel > 0)
        return;

    int pointSize = cursor.charFormat().font().pointSize();

    if (pointSize > 4) {
        QTextCharFormat charFmt;
        charFmt.setFontPointSize(qreal(pointSize - 1));
        mergeFormatOnSelection(charFmt);
    }
}

void TextEditor::clearStrongOnSelection()
{
    QTextCursor cursor = textCursor();

    // Without a selection, formatting only changes the current typing format
    if (!cursor.hasSelection()) {
        QTextCharFormat charFmt;
        charFmt.setFontWeight(blockDefaultFontWeight(cursor.block()));
        mergeFormatOnSelection(charFmt);
        return;
    }

    // Change text under cursor
    auto clearStrongModifier = [&](const QTextBlock &block, QTextCharFormat charFmt) {
        if (isMarkdownStrong(charFmt))
            charFmt.setFontWeight(blockDefaultFontWeight(block));
        return charFmt;
    };
    applyFragmentChangesToSelection(cursor, clearStrongModifier);
}

void TextEditor::applyHeadingCharFormat(const QTextBlock &block, int headingLevel)
{
    auto headingFormatModifier = [&](const QTextBlock &, QTextCharFormat charFmt){
        // Set / remove heading-specific visual formatting
        // depending on headingLevel (0 -> no heading)
        bool isStrong = isMarkdownStrong(charFmt);
        if (headingLevel > 0) {
            charFmt.setFontWeight(
                        isStrong
                        ? StrongFontWeight
                        : HeadingFontWeight);
            charFmt.setProperty(QTextCharFormat::Property::FontSizeAdjustment, 4 - headingLevel);
        } else {
            charFmt.setFontWeight(
                        isStrong
                        ? StrongFontWeight
                        : NormalFontWeight);
            charFmt.clearProperty(QTextCharFormat::Property::FontSizeAdjustment);
        }
        return charFmt;
    };
    applyFragmentChangesToBlock(block, headingFormatModifier);
}

void TextEditor::applyFragmentChangesToSelection(QTextCursor &cursor, const FormatModifier &modifier)
{
    cursor.beginEditBlock();
    applyFragmentChangesToRange(
                selectedBlocks(cursor),
                cursor.selectionStart(),
                cursor.selectionEnd(),
                modifier);
    cursor.endEditBlock();
}

void TextEditor::applyFragmentChangesToBlock(const QTextBlock &block, const FormatModifier &modifier)
{
    QTextCursor cursor(block);
    cursor.beginEditBlock();
    applyFragmentChangesToRange(
                {block},
                block.position(),
                block.position() + block.length(),
                modifier);
    cursor.endEditBlock();
}

void TextEditor::applyFragmentChangesToRange(const QVector<QTextBlock> &blocks,
                                             int startPos, int endPos,
                                             const FormatModifier &modifier)
{
    QVector<CharFormatUpdate> updates;

    for (const QTextBlock &block : blocks) {
        QTextBlock::iterator it = block.begin();
        while (!it.atEnd()) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid()) {
                int fragmentStart = fragment.position();
                int fragmentEnd = fragment.position() + fragment.length();

                int start = std::max<int>(startPos, fragmentStart);
                int end = std::min<int>(endPos, fragmentEnd);

                if (start < end) {
                    const QTextCharFormat charFmt = fragment.charFormat();
                    updates.append({start, end, modifier(block, charFmt)});
                }
            }
            ++it;
        }
    }

    QTextCursor localCursor(document());
    for (const CharFormatUpdate &update : updates) {
        localCursor.setPosition(update.start);
        localCursor.setPosition(update.end, QTextCursor::MoveMode::KeepAnchor);
        localCursor.setCharFormat(update.charFmt);
    }
}

void TextEditor::mergeFormatOnSelection(const QTextCharFormat &charFmt, bool selectWord)
{
    QTextCursor cursor = textCursor();

    // Begin edit block here
    // Thus, there won't be two undo actions after this function
    cursor.beginEditBlock();

    // It could make sense to select the word (or the line) under the cursor
    // if nothing else is selected while clicking bold, italic, etc.
    if (!cursor.hasSelection() && selectWord)
        cursor.select(QTextCursor::WordUnderCursor);

    // Merge char format
    cursor.mergeCharFormat(charFmt);
    mergeCurrentCharFormat(charFmt);

    // End edit block
    cursor.endEditBlock();
}

void TextEditor::insertHyperlink()
{
    QTextCursor cursor = textCursor();
    HyperlinkPtr link = getLinkUnderCursor(cursor);
    if (link) {
        // Edit link under cursor, if available
        m_editLinkAction->setData(QVariant::fromValue(link));
        editHyperlink();
        return;
    }

    LinkEditorDialog editor(tr("Insert link"));
    if (editor.exec() == QDialog::Accepted) {
        // Add hyperlink with specific char format
        QTextCursor cursor = textCursor();
        QTextCharFormat charFmt = cursor.charFormat();
        charFmt.setAnchor(true);
        charFmt.setAnchorHref(editor.linkUrl());
        charFmt.setForeground(linkColor());

        cursor.insertText(editor.caption(), charFmt);

        // Normalize char format after inserting the link
        charFmt = QTextCharFormat();
        charFmt.setAnchor(false);
        charFmt.setAnchorHref({});
        charFmt.setForeground(QColor("black"));
        cursor.mergeCharFormat(charFmt);
    }
}

void TextEditor::editHyperlink()
{
    LinkEditorDialog editor(tr("Edit link"));

    HyperlinkPtr link = m_editLinkAction->data().value<HyperlinkPtr>();
    editor.setLinkUrl(link->href);
    editor.setCaption(link->text);
    editor.selectCaption();

    if (editor.exec() == QDialog::Accepted) {
        QTextCursor cursor = textCursor();
        QTextCharFormat charFmt = cursor.charFormat();
        charFmt.setAnchor(true);
        charFmt.setAnchorHref(editor.linkUrl());
        charFmt.setForeground(linkColor());

        cursor.setPosition(link->position);
        cursor.setPosition(link->position + link->length, QTextCursor::KeepAnchor);
        cursor.insertText(editor.caption(), charFmt);

        charFmt = QTextCharFormat();
        charFmt.setAnchor(false);
        charFmt.setAnchorHref({});
        charFmt.setForeground(QColor("black"));
        cursor.mergeCharFormat(charFmt);
    }
}

void TextEditor::insertEmoji()
{
    // TODO: Implement insertEmoji
}

/*
 * Indent/Unindent affects selected list items.
 * Non-list paragraphs inside the selection are ignored.
 * If no selected block is a list item, the action does nothing.
 */
void TextEditor::adjustListIndentation(int delta)
{
    QTextCursor cursor = textCursor();
    QVector<QTextBlock> blocks = selectedBlocks(cursor);

    // Remove all blocks from selection that are not list items
    for (int i = blocks.size() - 1; i >= 0; --i) {
        const QTextBlock &block = blocks.at(i);
        if (!block.isValid() || !block.textList())
            blocks.removeAt(i);
    }

    if (blocks.isEmpty())
        return;

    cursor.beginEditBlock();
    for (const QTextBlock &block : blocks)
        adjustListBlockIndentation(block, delta);
    cursor.endEditBlock();

    emit blockFormatChanged();
}

void TextEditor::adjustListBlockIndentation(const QTextBlock &block, int delta)
{
    QTextBlockFormat blockFmt = block.blockFormat();
    int currentIndent = blockFmt.indent();
    int newIndent = std::max(0, currentIndent + delta);

    if (currentIndent == newIndent)
        return;

    blockFmt.setIndent(newIndent);

    QTextCursor blockCursor(block);
    blockCursor.setBlockFormat(blockFmt);

    if (newIndent > 0)
        setListStyle(blockCursor, LowerLevelListStyle);
    else
        setListStyle(blockCursor, TopLevelListStyle);
}

void TextEditor::setHeadingLevel(int level)
{
    QTextCursor cursor = textCursor();
    int currentLevel = cursor.blockFormat().headingLevel();
    if (currentLevel == level)
        // Do nothing
        return;

    cursor.beginEditBlock();

    QTextBlockFormat blockFmt = cursor.blockFormat();
    QTextList *textList = cursor.currentList();
    if (textList) {
        // Remove list
        blockFmt.setObjectIndex(-1);
        blockFmt.setIndent(0);
        blockFmt.clearProperty(QTextFormat::ListStyle);
    }

    // Set heading level for block
    blockFmt.setHeadingLevel(level);
    cursor.setBlockFormat(blockFmt);

    // Apply charformat modification
    applyHeadingCharFormat(cursor.block(), level);

    cursor.endEditBlock();

    emit blockFormatChanged();
}

void TextEditor::removeBlockStyle()
{
    QTextCursor cursor = textCursor();
    QVector<QTextBlock> blocks = selectedBlocks(cursor);

    cursor.beginEditBlock();
    for (const QTextBlock &block : blocks)
        removeBlockStyleFromBlock(block);
    cursor.endEditBlock();

    emit blockFormatChanged();
}

void TextEditor::removeBlockStyleFromBlock(const QTextBlock &block)
{
    QTextCursor blockCursor(block);
    QTextBlockFormat blockFmt = block.blockFormat();
    QTextList *textList = block.textList();

    if (textList) {
        textList->remove(block);
        blockFmt.setObjectIndex(-1);
        blockFmt.setIndent(0);
        blockFmt.clearProperty(QTextFormat::ListStyle);
    } else if (blockFmt.headingLevel() > 0) {
        blockFmt.setHeadingLevel(0);
        clearHeadingCharFormat(block);
    }

    blockCursor.setBlockFormat(blockFmt);
}

void TextEditor::toggleList()
{
    QTextCursor cursor = textCursor();
    const QTextBlock block = cursor.block();
    QTextBlockFormat blockFmt = block.blockFormat();

    cursor.beginEditBlock();

    if (blockFmt.headingLevel() > 0) {
        // Remove heading format
        blockFmt.setHeadingLevel(0);

        // Remove char format used for headings
        clearHeadingCharFormat(block);
    }

    QTextList *textList = cursor.currentList();
    if (!textList) {
        // Setup new list
        QTextListFormat listFmt;
        QTextListFormat::Style listStyle;
        listFmt.setIndent(1);
        if (blockFmt.indent() == 0)
            listStyle = TopLevelListStyle;
        else
            listStyle = LowerLevelListStyle;
        listFmt.setProperty(QTextFormat::ListStyle, listStyle);
        textList = cursor.createList(listFmt);

        blockFmt.setObjectIndex(textList->objectIndex());
    } else {
        // Remove list at current block
        textList->remove(block);

        blockFmt.setObjectIndex(-1);
        blockFmt.setIndent(0);
        // TODO: Check if QTextBlockFormat actually has QTextFormat::ListStyle as a property
        blockFmt.clearProperty(QTextFormat::ListStyle);
    }

    cursor.setBlockFormat(blockFmt);
    cursor.endEditBlock();

    emit blockFormatChanged();
}

void TextEditor::toggleBlockQuote()
{
    QTextCursor cursor = textCursor();
    const QTextBlock block = cursor.block();
    QTextBlockFormat blockFmt = block.blockFormat();

    cursor.beginEditBlock();

    if (blockFmt.headingLevel() > 0) {
        // Remove heading format
        blockFmt.setHeadingLevel(0);

        // Remove char format used for headings
        clearHeadingCharFormat(block);
    }

    QTextList *textList = cursor.currentList();
    if (textList) {
        // Remove list at current block
        textList->remove(block);

        blockFmt.setObjectIndex(-1);
        blockFmt.setIndent(0);
        // TODO: Check if QTextBlockFormat actually has QTextFormat::ListStyle as a property
        blockFmt.clearProperty(QTextFormat::ListStyle);
    }

    // Set / clear blockquote property
    if (blockFmt.hasProperty(QTextFormat::BlockQuoteLevel))
        blockFmt.clearProperty(QTextFormat::BlockQuoteLevel);
    else
        blockFmt.setProperty(QTextFormat::BlockQuoteLevel, 1);

    cursor.setBlockFormat(blockFmt);
    cursor.endEditBlock();

    emit blockFormatChanged();
}

void TextEditor::makeHorizontalRuler()
{
    QTextCursor cursor = textCursor();
    const QTextBlock block = cursor.block();
    QTextBlockFormat blockFmt = block.blockFormat();

    cursor.beginEditBlock();

    if (blockFmt.headingLevel() > 0)
        // Remove heading format
        blockFmt.setHeadingLevel(0);

    QTextList *textList = cursor.currentList();
    if (textList) {
        // Remove list property at current block
        textList->remove(block);
        blockFmt.setObjectIndex(-1);
    }

    // Set horizontal ruler property
    blockFmt.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
#ifdef HORIZONTAL_RULER_COLOR
    blockFmt.setProperty(QTextFormat::BackgroundBrush, horizontalRulerColor());
#endif

    // Remove line
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.removeSelectedText();

    // Update block format
    cursor.setBlockFormat(blockFmt);

    // Add new line
    cursor.insertBlock(defaultBlockFormat(), defaultCharFormat());

    cursor.endEditBlock();
}

void TextEditor::setListStyle(QTextCursor &cursor, QTextListFormat::Style style)
{
    // Check if styles are different
    QTextBlockFormat blockFmt = cursor.blockFormat();
    QTextListFormat::Style oldStyle = static_cast<QTextListFormat::Style>(
                blockFmt.property(QTextFormat::ListStyle).toInt());

    if (style == oldStyle)
        return;

    // Change list style
    blockFmt.setProperty(QTextFormat::ListStyle, style);
    cursor.setBlockFormat(blockFmt);
}

QVector<QTextBlock> TextEditor::selectedBlocks(const QTextCursor &cursor) const
{
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    if (start == end)
        return {cursor.block()};

    QTextDocument *doc = document();

    QTextBlock firstBlock = doc->findBlock(start);
    QTextBlock lastBlock = doc->findBlock(end);

    //  If the selection ends exactly at the beginning of a block, that block
    //  is not part of the user's visible selection.
    if (end == lastBlock.position() && lastBlock != firstBlock)
        lastBlock = lastBlock.previous();

    QVector<QTextBlock> blocks;
    QTextBlock block = firstBlock;
    while (block.isValid()) {
        blocks.append(block);
        if (block == lastBlock)
            break;
        block = block.next();
    }

    return blocks;
}

HyperlinkPtr TextEditor::getLinkUnderCursor(const QTextCursor &cursor)
{
    const QTextCharFormat charFmt = cursor.charFormat();
    if (!charFmt.isAnchor())
        return {};

    int curPos = cursor.position();

    bool containsLink = false;
    QTextBlock block = cursor.block();

    QTextBlock::iterator it = block.begin();
    QTextFragment fragment;
    while (!it.atEnd()) {
        fragment = it.fragment();
        if (fragment.contains(curPos)) {
            containsLink = true;
            break;
        }
        ++it;
    }

    if (!containsLink)
        return {};

    if (!fragment.charFormat().isAnchor())
        return {};

    int firstPos = fragment.position();
    int lastPos = fragment.position() + fragment.length();
    QString text = fragment.text();

    return std::make_shared<Hyperlink>(firstPos, lastPos - firstPos, text, charFmt.anchorHref());
}

void TextEditor::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Tab:
        indentSelection();
        return;
    case Qt::Key_Backtab:
        unindentSelection();
        return;
    case Qt::Key_C:
        if (event->modifiers() == Qt::ControlModifier) {
            copy();
            return;
        }
        break;
    case Qt::Key_V:
        if (event->modifiers() == Qt::ControlModifier) {
            paste();
            return;
        }
        break;
    case Qt::Key_X:
        if (event->modifiers() == Qt::ControlModifier) {
            cut();
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        insertBlock();
        return;
    case Qt::Key_Space: {
        QTextCursor cursor = textCursor();
        const QTextBlock block = cursor.block();
        QTextList *textList = cursor.currentList();
        if (!textList && cursor.position() - block.position() == 1) {
            auto it = block.begin();
            if (it.fragment().text().startsWith(QStringLiteral("*"))) {
                // Create a new list if a line starts with "* "
                // First: remove the asterisk
                QTextCursor localCursor(cursor);
                localCursor.beginEditBlock();
                localCursor.setPosition(block.position());
                localCursor.setPosition(block.position() + 1, QTextCursor::MoveMode::KeepAnchor);
                localCursor.removeSelectedText();

                // Second: create a list
                toggleList();
                localCursor.endEditBlock();

                return;
            }
            else if (it.fragment().text().startsWith(QStringLiteral(">"))) {
                // Create a blockquote if a line starts with "> "
                QTextCursor localCursor(cursor);
                localCursor.beginEditBlock();
                localCursor.setPosition(block.position());
                localCursor.setPosition(block.position() + 1, QTextCursor::MoveMode::KeepAnchor);
                localCursor.removeSelectedText();

                // Second: create a list
                toggleBlockQuote();
                localCursor.endEditBlock();

                return;
            }
        }
    }
    case Qt::Key_Backspace: {
        QTextCursor cursor = textCursor();
        const QTextBlock block = cursor.block();
        QTextBlockFormat blockFmt = block.blockFormat();
        if (cursor.position() == block.position()) {
            QTextList *textList = cursor.currentList();
            if (textList) {
                // Remove the list bullet from the current block
                toggleList();
                return;
            } else if (blockFmt.hasProperty(QTextFormat::BlockQuoteLevel)) {
                toggleBlockQuote();
                return;
            }
        }
    }
    case Qt::Key_Minus: {
        QTextCursor cursor = textCursor();
        const QTextBlock block = cursor.block();
        QTextList *textList = cursor.currentList();
        if (!textList && cursor.position() - block.position() == 2 && block.length() == 3) {
            auto it = block.begin();
            if (it.fragment().text().startsWith(QStringLiteral("--"))) {
                // Create a horizontal ruler if the line starts with three dashes
                makeHorizontalRuler();
                return;
            }
        }
    }
    default:
        ;
    }

    m_ctrlPressed = event->modifiers() & Qt::ControlModifier;
    m_anchor = anchorAt(viewport()->mapFromGlobal(QCursor::pos()));

    updateOverrideCursor();

    QTextEdit::keyPressEvent(event);
}

void TextEditor::updateCursorX(const QTextCursor &cursor)
{
    const QTextBlock block = cursor.block();
    const QTextLayout *layout = block.layout();

    int pos = cursor.position() - block.position();

    const QTextLine line = layout->lineForTextPosition(pos);
    if (line.isValid())
        m_cursorX = line.cursorToX(pos);
    else
        m_cursorX = -1;
}

void TextEditor::keyReleaseEvent(QKeyEvent *event)
{
    if (m_ctrlPressed && ~event->modifiers() & Qt::ControlModifier) {
        m_ctrlPressed = false;
        QGuiApplication::setOverrideCursor(Qt::ArrowCursor);
    }
    QTextEdit::keyReleaseEvent(event);
}

void TextEditor::mouseMoveEvent(QMouseEvent *event)
{
    handleMouseEvent(event);
    updateOverrideCursor();

    QTextEdit::mouseMoveEvent(event);
}

void TextEditor::mousePressEvent(QMouseEvent *event)
{
    handleMouseEvent(event);
    updateOverrideCursor();

    QTextEdit::mousePressEvent(event);
}

void TextEditor::mouseReleaseEvent(QMouseEvent *event)
{
    handleMouseEvent(event);

    if (m_anchor.isEmpty()) {
        // If no link is being clicked
        // Use default event handler
        QTextEdit::mouseReleaseEvent(event);
        return;
    }

    if (!m_ctrlPressed)
        // If Ctrl is not pressed, do nothing!
        return;

    if (m_anchor.length() > 8 && m_anchor.left(8) == QStringLiteral("https://")) {
        QUrl url(m_anchor);

        m_ctrlPressed = false;
        m_anchor.clear();
        updateOverrideCursor();

        // Open url in standard browser
        QDesktopServices::openUrl(url);
    } else if (m_anchor.length() > 7 && m_anchor.left(7) == QStringLiteral("file://")) {
        const QString filename = m_anchor.mid(7);

        m_ctrlPressed = false;
        m_anchor.clear();
        updateOverrideCursor();

        // Open filename with standard application
        systemOpenFile(filename, m_workingDirPath);
    }
}

void TextEditor::handleMouseEvent(QMouseEvent *event)
{
    if (!m_ctrlPressed && event->modifiers() & Qt::ControlModifier)
        m_ctrlPressed = true;
    m_anchor = anchorAt(event->pos());
}

void TextEditor::updateOverrideCursor() const
{
    if (m_ctrlPressed && !m_anchor.isEmpty())
        QGuiApplication::setOverrideCursor(Qt::PointingHandCursor);
    else
        QGuiApplication::setOverrideCursor(Qt::ArrowCursor);
}

void TextEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    QTextCursor cursor = textCursor();
    QTextCursor localCursor = cursorForPosition(event->pos());

    // Use default event handler, if the cursor is not over a link
    QTextCharFormat charFmt = localCursor.charFormat();
    if (!charFmt.isAnchor())
        return QTextEdit::mouseDoubleClickEvent(event);
    HyperlinkPtr hyperlink = getLinkUnderCursor(localCursor);
    if (!hyperlink)
        return QTextEdit::mouseDoubleClickEvent(event);

    // Select either word under cursor or the complete hyperlink caption
    if (cursor.selectionStart() == hyperlink->position &&
        cursor.selectionEnd() == hyperlink->position + hyperlink->length)
        localCursor.select(QTextCursor::WordUnderCursor);
    else {
        localCursor.setPosition(hyperlink->position);
        localCursor.setPosition(hyperlink->position + hyperlink->length,
                                QTextCursor::KeepAnchor);
    }
    setTextCursor(localCursor);
}

void TextEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QTextCursor cursor = cursorForPosition(event->pos());
    HyperlinkPtr hyperlink = getLinkUnderCursor(cursor);
    bool isLink = bool(hyperlink);

    m_editLinkAction->setData(QVariant::fromValue(hyperlink));
    m_editLinkAction->setVisible(isLink);
    m_editLinkAction->setEnabled(isLink);
    m_contextMenuSeparator->setVisible(isLink);

    m_contextMenu->exec(event->globalPos());
}

/*
 * Ensure custom behavior when adding a new line with keyboard.
 */
void TextEditor::insertBlock()
{
    QTextCursor cursor = textCursor();
    QTextBlockFormat blockFmt = cursor.blockFormat();
    int posInBlock = cursor.positionInBlock();

    cursor.beginEditBlock();
    if (posInBlock == 0) {
        cursor.setCharFormat(defaultCharFormat());

        // The cursor is at the beginning of the line
        // Thus all the content is going to move to the newly inserted line
        // -> Move the block and char formatting as well
        cursor.insertBlock();

        // If previous block has been a heading or a blockquote
        // Set default format there
        if (blockFmt.headingLevel() > 0 || blockFmt.hasProperty(QTextFormat::BlockQuoteLevel)) {
            cursor.setPosition(cursor.position() - 1);
            cursor.setBlockFormat(defaultBlockFormat());
        }
    } else if (blockFmt.objectIndex() != -1)
        cursor.insertBlock(blockFmt);
    else
        cursor.insertBlock(defaultBlockFormat(), defaultCharFormat());
    cursor.endEditBlock();

    ensureCursorVisible();
}

/*
 * Override the copy function to generate custom HTML mime data that includes indent levels,
 * headings, and list points.
 */
void TextEditor::copy()
{
    QMimeData *mimeData = new QMimeData;
    QTextCursor cursor = QTextCursor();

    // Export text as HTML
    QString selectionAsHtml = HtmlExporter::exportDocument(document(), &cursor);
    mimeData->setHtml(selectionAsHtml);

    // Export text as plain
    QTextDocumentFragment fragment(cursor);
    mimeData->setText(fragment.toPlainText());

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

/*
 * Copy the whole document as a Markdown string.
 */
void TextEditor::copyAsMarkdown()
{
    QMimeData *mimeData = new QMimeData;

    // Export text as Markdown
    QString markdown = MarkdownExporter::exportDocument(document());
    mimeData->setText(markdown);
    mimeData->setData("text/markdown", markdown.toUtf8());

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void TextEditor::paste()
{
    QTextCursor cursor = textCursor();
    QTextDocumentFragment fragment;

    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData->hasHtml()) {
        QTextDocument *contentDoc = documentFromHtml(mimeData->html());
        fragment = QTextDocumentFragment(contentDoc);
    } else {
        QString text = mimeData->text();
        if (!text.isNull())
            fragment = QTextDocumentFragment::fromPlainText(text);
    }

    if (!fragment.isEmpty()) {
        cursor.insertFragment(fragment);
        ensureCursorVisible();
    }
}

/*
 * Analogous to the copy function - just with selection removal after the operation.
 */
void TextEditor::cut()
{
    copy();

    QTextCursor cursor = textCursor();
    cursor.removeSelectedText();
}

void TextEditor::updatePasteAction()
{
    m_pasteAction->setEnabled(canPaste());
}

void systemOpenFile(const QString &filename, const QString &dirPath)
{
    const QString workingDirectory =
        dirPath.isEmpty() ? QDir::currentPath() : QDir(dirPath).absolutePath();

    QString program;
    QStringList arguments;

#ifdef Q_OS_MACOS
    program = QStringLiteral("open");
    arguments << filename;
#elif defined(Q_OS_WIN)
    program = QStringLiteral("cmd.exe");
    arguments << QStringLiteral("/c")
              << QStringLiteral("start")
              << QStringLiteral("")
              << filename;
#else
    program = QStringLiteral("xdg-open");
    arguments << filename;
#endif

    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    process.start(program, arguments);

    if (!process.waitForStarted()) {
        qWarning().noquote()
            << QStringLiteral("Error opening file \"%1\".").arg(filename);
        return;
    }

    process.waitForFinished();
}
