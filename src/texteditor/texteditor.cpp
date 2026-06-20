#include "texteditor.h"
#include "texteditor_p.h"

#include "blocktypes.h"
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

TextEditor::TextEditor(QWidget *parent)
    : QTextEdit(parent),
      m_ctrlPressed(false)
{
    // Connect signals to update actions / char format
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

/*
 * Override the copy function to generate custom HTML mime data that includes indent levels,
 * headings, and list points.
 */
void TextEditor::copy()
{
    QMimeData *mimeData = new QMimeData;
    QTextCursor cursor = textCursor();

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

void TextEditor::onCurrentCharFormatChanged(const QTextCharFormat &format)
{
    emit fontChanged(format.font());

    m_boldAction->setChecked(isStrong(format));
    m_italicAction->setChecked(format.fontItalic());
    m_underlineAction->setChecked(format.fontUnderline());
}

void TextEditor::onCursorPositionChanged()
{
    const QTextCursor cursor = textCursor();

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
    } else {
        const QTextCursor cursor = textCursor();
        if (!cursor.hasSelection()) {
            // Without a selection, formatting only changes the current typing format
            QTextCharFormat charFmt;
            charFmt.setFontWeight(blockDefaultFontWeight(cursor.block()));
            mergeFormatOnSelection(charFmt);
        } else {
            // Since headings and paragraphs have different default weights
            // Clearing bold/strong requires fragment-wise changes
            auto clearStrongModifier = [&](const QTextBlock &block, QTextCharFormat charFmt) {
                if (isStrong(charFmt))
                    charFmt.setFontWeight(blockDefaultFontWeight(block));
                return charFmt;
            };
            applyCharFormatModifier(cursor, clearStrongModifier);
        }
    }
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
        adjustListIndentationForBlock(block, delta);
    cursor.endEditBlock();
}

void TextEditor::adjustListIndentationForBlock(const QTextBlock &block, int delta)
{
    QTextBlockFormat blockFmt = block.blockFormat();
    int currentIndent = blockFmt.indent();
    int newIndent = std::max(0, currentIndent + delta);

    if (currentIndent == newIndent)
        return;

    // Nesting is stored per list item as QTextBlockFormat::indent().
    blockFmt.setIndent(newIndent);

    // Setting the ListStyle property for QTextBlockFormat
    // overrides the QTextListFormat::Style
    blockFmt.setProperty(QTextFormat::ListStyle, newIndent > 0
                         ? LowerLevelListStyle
                         : TopLevelListStyle);

    QTextCursor blockCursor(block);
    blockCursor.setBlockFormat(blockFmt);

    emit blockFormatChanged(block);
}

/*
 * Ensure custom behavior when adding a new line with keyboard.
 */
void TextEditor::insertBlock()
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    BlockType type = blockType(block);
    int posInBlock = cursor.positionInBlock();

    cursor.beginEditBlock();
    if (posInBlock == 0 && cursor.selectionStart() == cursor.selectionEnd()) {
        // The cursor is at the beginning of the line
        // Move all char and block formatting to the new line
        cursor.insertBlock();

        // If previous block has been a heading or a blockquote
        // Set default format there
        if (isHeading(type) || type == BlockType::BlockQuote) {
            cursor.setPosition(cursor.position() - 1);
            cursor.setBlockFormat(defaultBlockFormat());
            cursor.setBlockCharFormat(defaultCharFormat());
        }
    } else if (type == BlockType::TextList)
        cursor.insertBlock(block.blockFormat());
    else
        cursor.insertBlock(defaultBlockFormat(), defaultCharFormat());
    cursor.endEditBlock();

    ensureCursorVisible();
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
        BlockType type = blockType(block);
        if (type == BlockType::Paragraph) {
            static const QMap<QString, BlockType> typeMarker = {
                {QStringLiteral("*"), BlockType::TextList},
                {QStringLiteral(">"), BlockType::BlockQuote}
            };
            static const QList<QString> keys = typeMarker.keys();

            BlockType newType = type;
            auto it = block.begin();
            const QStringView text(it.fragment().text());
            int markerLength = 0;
            for (const QString &marker : keys) {
                if (cursor.position() - block.position() == marker.length()
                        && text.startsWith(marker)) {
                    newType = typeMarker.value(marker);
                    markerLength = marker.length();
                    break;
                }
            }

            if (newType != type) {
                // Create a new block format if line starts with a specifc marker
                QTextCursor localCursor(cursor);
                localCursor.beginEditBlock();
                localCursor.setPosition(block.position());
                localCursor.setPosition(block.position() + markerLength, QTextCursor::MoveMode::KeepAnchor);
                localCursor.removeSelectedText();
                setBlockTypeForBlock(block, newType);
                localCursor.endEditBlock();
                emit blockFormatChanged(block);
                return;
            }
        }
        break;
    }
    case Qt::Key_Backspace: {
        QTextCursor cursor = textCursor();
        const QTextBlock block = cursor.block();
        if (cursor.position() == block.position()) {
            BlockType type = blockType(block);
            if (type == BlockType::TextList || type == BlockType::BlockQuote) {
                cursor.beginEditBlock();
                cursor.removeSelectedText();
                setBlockTypeForBlock(block, BlockType::Paragraph);
                cursor.endEditBlock();
                emit blockFormatChanged(block);
                return;
            }
        }
        break;
    }
    case Qt::Key_Delete: {
        QTextCursor cursor = textCursor();
        QTextBlockFormat blockFmt = cursor.blockFormat();
        QTextEdit::keyPressEvent(event);
        if (blockFmt != cursor.blockFormat())
            emit blockFormatChanged(cursor.block());
        return;
    }
    case Qt::Key_Minus: {
        QTextCursor cursor = textCursor();
        const QTextBlock block = cursor.block();
        QTextList *textList = cursor.currentList();
        if (!textList && cursor.position() - block.position() == 2 && block.length() == 3) {
            auto it = block.begin();
            if (it.fragment().text().startsWith(QStringLiteral("--"))) {
                // Create a horizontal ruler if the line starts with three dashes
                setBlockTypeForBlock(block, BlockType::HorizontalRuler);
                insertBlock();
                emit blockFormatChanged(block);
                return;
            }
        }
        break;
    }
    default:
        ;
    }

    m_ctrlPressed = event->modifiers() & Qt::ControlModifier;
    m_anchor = anchorAt(viewport()->mapFromGlobal(QCursor::pos()));

    updateOverrideCursor();

    QTextEdit::keyPressEvent(event);
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
