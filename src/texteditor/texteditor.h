#ifndef TEXTEDITOR_H
#define TEXTEDITOR_H

#include <QTextEdit>
#include <memory>

class QAction;
class QFont;
class QMenu;
class QString;
class QTextBlock;
class QTextCharFormat;
class QTextDocument;

struct Hyperlink;
typedef std::shared_ptr<Hyperlink> HyperlinkPtr;

class TextEditor : public QTextEdit
{
    Q_OBJECT
public:
    explicit TextEditor(QWidget *parent = nullptr);

    // Import document
    void setDocument(QTextDocument *document);

    // Copy content as markdown to clipboard
    void copyAsMarkdown();

    // Working directory for opening files
    void setWorkingDirectory(const QString &dirPath)
    { m_workingDirPath = dirPath; }

signals:
    void fontChanged(const QFont &font);
    void blockFormatChanged();

private:
    void setupActions();
    void setupContextMenu();

private slots:
    // QTextEdit slots
    void onCurrentCharFormatChanged(const QTextCharFormat &format);
    void onCursorPositionChanged();
    void onSelectionChanged();

    // CharFormat manipulation
    void updateBold();
    void updateItalic();
    void updateUnderline();
    void setTextSize(const QString &sizeText);
    void textsizePlus();
    void textsizeMinus();

private:
    void clearStrongOnSelection();

    void applyHeadingCharFormat(const QTextBlock &block, int headingLevel);
    void clearHeadingCharFormat(const QTextBlock &block)
    { applyHeadingCharFormat(block, 0); }

    typedef std::function<QTextCharFormat(const QTextBlock &, QTextCharFormat)> FormatModifier;
    struct CharFormatUpdate {
        int start;
        int end;
        QTextCharFormat charFmt;
    };

    void applyFragmentChangesToSelection(QTextCursor &cursor, const FormatModifier &modifier);
    void applyFragmentChangesToBlock(const QTextBlock &block, const FormatModifier &modifier);
    void applyFragmentChangesToRange(const QVector<QTextBlock> &blocks,
                                     int startPos, int endPos,
                                     const FormatModifier &modifier);

    void mergeFormatOnSelection(const QTextCharFormat &format, bool selectWord = false);

private slots:
    // Text insertion
    void insertHyperlink();
    void editHyperlink();
    void insertEmoji();

private:
    // BlockFormat manipulation
    void indentSelection() { adjustListIndentation(1); }
    void unindentSelection() { adjustListIndentation(-1); }
    void adjustListIndentation(int delta);
    void adjustListBlockIndentation(const QTextBlock &block, int delta);

    void setHeadingLevel(int level);

    void removeBlockStyle();
    void removeBlockStyleFromBlock(const QTextBlock &block);

    void toggleList();
    void makeHorizontalRuler();

    void insertListPadding(const QTextBlock &block) const;
    void removeListPadding(const QTextBlock &block) const;

    void setListStyle(QTextCursor &cursor, QTextListFormat::Style style);

    QVector<QTextBlock> selectedBlocks(const QTextCursor &cursor) const;

    static HyperlinkPtr getLinkUnderCursor(const QTextCursor &cursor);

    // Event handlers
    void keyPressEvent(QKeyEvent *event) override;
    bool moveCursor(QTextCursor::MoveOperation op);
    void updateCursorX(const QTextCursor &cursor);
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void handleMouseEvent(QMouseEvent *event);
    void updateOverrideCursor() const;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    // Custom editor behavior
    void insertBlock();

    // Clipboard functions
    void copy();
    void paste();
    void cut();

    void updatePasteAction();

    // Standard richtext actions
    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_cutAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_removeAction;
    QAction *m_boldAction;
    QAction *m_italicAction;
    QAction *m_underlineAction;

    // Advanced text actions
    QAction *m_textsizePlusAction;
    QAction *m_textsizeMinusAction;
    QAction *m_lessIndentAction;
    QAction *m_moreIndentAction;
    QAction *m_editLinkAction;
    QAction *m_insertEmojiAction;

    // Menu separator (only visible when mouse is over a link)
    QAction *m_contextMenuSeparator;

    // Context menu (opens on left click)
    QMenu *m_contextMenu;

    // X coordinate of the cursor for navigating with Up/Down keys
    qreal m_cursorX;
    bool m_keepCursorX;

    // Hyperlink related keyboard / mouse event properties
    bool m_ctrlPressed;
    QString m_anchor;

    QString m_workingDirPath;

    friend class TextEditorWidget;
};

#endif
