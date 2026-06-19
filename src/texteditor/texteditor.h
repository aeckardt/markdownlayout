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

private:
    // Initialization (called from constructor)
    void setupActions();
    void setupContextMenu();

public:
    // Import document
    void setDocument(QTextDocument *document);

    enum BlockType : int {
        Paragraph       = 0,
        Heading1        = 10,
        Heading2        = 11,
        Heading3        = 12,
        Heading4        = 13,
        TextList        = 20,
        BlockQuote      = 30,
        HorizontalRuler = 40
    };

    BlockType blockType(const QTextBlock &block) const;
    static int headingLevel(BlockType type);

    // Copy content as markdown to clipboard
    void copyAsMarkdown();

    // Working directory for opening files
    void setWorkingDirectory(const QString &dirPath)
    { m_workingDirPath = dirPath; }

public slots:
    // Clipboard functions
    void copy();
    void paste();
    void cut();

    // Text insertion
    void insertHyperlink();
    void editHyperlink();
    void insertEmoji();

signals:
    void fontChanged(const QFont &font);
    void blockFormatChanged(const QTextBlock &block);

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

    void setHeadingCharFormat(const QTextBlock &block, int headingLevel);
    void clearHeadingCharFormat(const QTextBlock &block)
    { setHeadingCharFormat(block, 0); }

    typedef std::function<QTextCharFormat(const QTextBlock &, QTextCharFormat)> FormatModifier;
    struct CharFormatUpdate {
        int start;
        int end;
        QTextCharFormat newCharFmt;
    };

    void applyFragmentChangesToSelection(const FormatModifier &modifier);
    void applyFragmentChangesToBlock(const QTextBlock &block, const FormatModifier &modifier);
    void applyFragmentChangesToRange(const QVector<QTextBlock> &blocks,
                                     int startPos, int endPos,
                                     const FormatModifier &modifier);

    void mergeFormatOnSelection(const QTextCharFormat &format, bool selectWord = false);

    // BlockFormat manipulation
    enum ScopePolicy {
        CurrentBlock,
        SelectedBlocks
    };

    void setBlockType(BlockType type, ScopePolicy policy, bool toggle = false);
    void setBlockTypeForBlock(const QTextBlock &block, BlockType type, bool toggle = false);

    void setHeadingLevel(int level)
    { setBlockType(static_cast<BlockType>((int)Heading1 + (level - 1)), CurrentBlock); }

    void toggleList()
    { setBlockType(TextList, SelectedBlocks, true); }

    void toggleBlockQuote()
    { setBlockType(BlockQuote, SelectedBlocks, true); }

    void makeHorizontalRuler()
    { setBlockType(HorizontalRuler, CurrentBlock); }

    void removeBlockStyle()
    { setBlockType(Paragraph, SelectedBlocks); }

    void indentSelection() { adjustListIndentation(1); }
    void unindentSelection() { adjustListIndentation(-1); }
    void adjustListIndentation(int delta);
    void adjustListIndentationForBlock(const QTextBlock &block, int delta);

    QVector<QTextBlock> selectedBlocks() const;

    // Custom editor behavior
    void insertBlock();

    static HyperlinkPtr getLinkUnderCursor(const QTextCursor &cursor);

    // Event handlers
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void handleMouseEvent(QMouseEvent *event);
    void updateOverrideCursor() const;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

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

    // Hyperlink related keyboard / mouse event properties
    bool m_ctrlPressed;
    QString m_anchor;

    QString m_workingDirPath;

    friend class TextEditorWidget;
};

#endif
