#include "texteditor/texteditor.h"

#include "texteditor/html/exporter.h"
#include "texteditor/markdown/exporter.h"
#include "texteditor/markdown/importer.h"
#include "texteditor/style.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFormat>
#include <QTextList>

#include <algorithm>

namespace notetree::texteditor {

TextEditor::TextEditor(QWidget *parent)
    : QTextEdit(parent)
{
    connect(this, &QTextEdit::currentCharFormatChanged,
            this, &TextEditor::onCurrentCharFormatChanged);
    connect(this, &QTextEdit::cursorPositionChanged,
            this, &TextEditor::onCursorPositionChanged);
    connect(this, &QTextEdit::selectionChanged,
            this, &TextEditor::onSelectionChanged);

    setupActions();

    setViewportMargins(viewportMargin(), viewportMargin(), viewportMargin(), viewportMargin());
    setStyleSheet(QStringLiteral("QTextEdit { background: white; }"));
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

    m_deleteAction = new QAction(tr("Delete"), this);
    m_deleteAction->setEnabled(false);
    connect(m_deleteAction, &QAction::triggered, this, [this] { textCursor().removeSelectedText(); });

    m_boldAction = new QAction(tr("Bold"), this);
    m_boldAction->setCheckable(true);
    m_boldAction->setShortcut(QKeySequence::Bold);
    connect(m_boldAction, &QAction::triggered, this, &TextEditor::toggleBold);
    addAction(m_boldAction);

    m_italicAction = new QAction(tr("Italic"), this);
    m_italicAction->setCheckable(true);
    m_italicAction->setShortcut(QKeySequence::Italic);
    connect(m_italicAction, &QAction::triggered, this, &TextEditor::toggleItalic);
    addAction(m_italicAction);

    m_underlineAction = new QAction(tr("Underline"), this);
    m_underlineAction->setCheckable(true);
    m_underlineAction->setShortcut(QKeySequence::Underline);
    connect(m_underlineAction, &QAction::triggered, this, &TextEditor::toggleUnderline);
    addAction(m_underlineAction);

    m_contextMenu = new QMenu(this);
    m_contextMenu->addAction(m_undoAction);
    m_contextMenu->addAction(m_redoAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_cutAction);
    m_contextMenu->addAction(m_copyAction);
    m_contextMenu->addAction(m_pasteAction);
    m_contextMenu->addAction(m_deleteAction);
}

void TextEditor::setDocumentWithDefaults(QTextDocument *document)
{
    if (!document)
        return;
    document->setIndentWidth(documentIndentWidth());
    QTextEdit::setDocument(document);
    m_undoAction->setEnabled(document->isUndoAvailable());
    m_redoAction->setEnabled(document->isRedoAvailable());
    moveCursor(QTextCursor::Start);
    ensureCursorVisible();
}

QString TextEditor::toMarkdown() const
{
    return MarkdownExporter::exportDocument(const_cast<QTextDocument *>(document()));
}

QString TextEditor::toHtmlFragment() const
{
    return HtmlExporter::exportDocument(const_cast<QTextDocument *>(document()), nullptr, true);
}

void TextEditor::insertMarkdown(const QString &markdown)
{
    auto imported = MarkdownImporter::importMarkdown(markdown);
    textCursor().insertFragment(QTextDocumentFragment(imported.get()));
}

void TextEditor::toggleBold()
{
    if (m_boldAction->isChecked())
        setMarkdownStrongOnSelection();
    else
        clearMarkdownStrongOnSelection();
}

void TextEditor::toggleItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(m_italicAction->isChecked());
    mergeFormatOnSelection(fmt);
}

void TextEditor::toggleUnderline()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(m_underlineAction->isChecked());
    mergeFormatOnSelection(fmt);
}

void TextEditor::changeTextSize(const QString &sizeText)
{
    if (textCursor().blockFormat().headingLevel() > 0)
        return;
    bool ok = false;
    const double pointSize = sizeText.toDouble(&ok);
    if (!ok || pointSize <= 0)
        return;
    QTextCharFormat fmt;
    fmt.setFontPointSize(pointSize);
    mergeFormatOnSelection(fmt);
}

void TextEditor::setHeadingLevel(int level)
{
    level = std::clamp(level, 0, 4);
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setHeadingLevel(level);
    cursor.setBlockFormat(blockFormat);

    QTextCharFormat charFormat;
    if (level > 0) {
        charFormat.setFontWeight(HeadingFontWeight);
        charFormat.setProperty(QTextFormat::FontSizeAdjustment, 4 - level);
    } else {
        charFormat.setFontWeight(NormalFontWeight);
        charFormat.clearProperty(QTextFormat::FontSizeAdjustment);
    }
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.mergeCharFormat(charFormat);

    cursor.endEditBlock();
    setTextCursor(cursor);
    emit blockFormatChanged();
}

void TextEditor::setParagraph()
{
    setHeadingLevel(0);
}

void TextEditor::mergeFormatOnSelection(const QTextCharFormat &format, bool selectWord)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    if (!cursor.hasSelection() && selectWord)
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    mergeCurrentCharFormat(format);
    cursor.endEditBlock();
}

void TextEditor::setMarkdownStrongOnSelection()
{
    QTextCharFormat fmt;
    fmt.setFontWeight(StrongFontWeight);
    mergeFormatOnSelection(fmt);
}

void TextEditor::clearMarkdownStrongOnSelection()
{
    QTextCursor cursor = textCursor();
    QTextCharFormat fmt;
    fmt.setFontWeight(blockDefaultFontWeight(cursor.block()));
    mergeFormatOnSelection(fmt);
}

void TextEditor::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_contextMenu)
        m_contextMenu->exec(event->globalPos());
}

void TextEditor::onCurrentCharFormatChanged(const QTextCharFormat &format)
{
    emit fontChanged(format.font());
    if (m_boldAction)
        m_boldAction->setChecked(isMarkdownStrong(format));
    if (m_italicAction)
        m_italicAction->setChecked(format.fontItalic());
    if (m_underlineAction)
        m_underlineAction->setChecked(format.fontUnderline());
}

void TextEditor::onCursorPositionChanged()
{
    emit blockFormatChanged();
}

void TextEditor::onSelectionChanged()
{
    const bool hasSelection = textCursor().hasSelection();
    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_deleteAction->setEnabled(hasSelection);
}

void TextEditor::updatePasteAction()
{
    if (m_pasteAction)
        m_pasteAction->setEnabled(canPaste());
}

} // namespace notetree::texteditor
