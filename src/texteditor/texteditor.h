#ifndef TEXTEDITOR_H
#define TEXTEDITOR_H

#include <QFont>
#include <QTextCharFormat>
#include <QTextEdit>

class QAction;
class QMenu;

namespace notetree::texteditor {

class TextEditor : public QTextEdit {
    Q_OBJECT
public:
    explicit TextEditor(QWidget *parent = nullptr);

    void setDocumentWithDefaults(QTextDocument *document);
    QString toMarkdown() const;
    QString toHtmlFragment() const;

signals:
    void fontChanged(const QFont &font);
    void blockFormatChanged();

public slots:
    void toggleBold();
    void toggleItalic();
    void toggleUnderline();
    void changeTextSize(const QString &sizeText);
    void setHeadingLevel(int level);
    void setParagraph();
    void insertMarkdown(const QString &markdown);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onCurrentCharFormatChanged(const QTextCharFormat &format);
    void onCursorPositionChanged();
    void onSelectionChanged();
    void updatePasteAction();

private:
    void setupActions();
    void mergeFormatOnSelection(const QTextCharFormat &format, bool selectWord = false);
    void setMarkdownStrongOnSelection();
    void clearMarkdownStrongOnSelection();
    void applyHeadingCharFormat(const QTextBlock &block, int headingLevel);

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_boldAction = nullptr;
    QAction *m_italicAction = nullptr;
    QAction *m_underlineAction = nullptr;
    QMenu *m_contextMenu = nullptr;
};

} // namespace notetree::texteditor

#endif
