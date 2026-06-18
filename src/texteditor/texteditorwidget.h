#ifndef TEXTEDITORWIDGET_H
#define TEXTEDITORWIDGET_H

#include <QWidget>

class QComboBox;
class QTextBlock;
class QToolBar;

class GradientButton;
class TextEditor;

class TextEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TextEditorWidget(QWidget *parent = nullptr);

    TextEditor &editor() { return *m_textEdit; }
    const TextEditor &editor() const { return *m_textEdit; }

private:
    typedef std::function<void()> ClickHandler;

    // ToolBar initialization
    void setupToolBar();

    GradientButton *addToolButton(const QString &iconName, QAction *action);
    GradientButton *addExtToolButton(const QString &iconName, const ClickHandler &clickFnc = {},
                                     bool enabled = true, bool checkable = false,
                                     const QString &tooltip = {});
    void addSeparator();

private slots:
    void onFontChanged(const QFont &font);
    void onBlockFormatChanged(const QTextBlock &block);
    void setHeadingLevel(int level);
    void updateBlockFormatButtons();

private:
    static QImage loadIconImage(const QString &iconName);

    QToolBar *m_toolbar;
    TextEditor *m_textEdit;

    // Char format style options
    GradientButton *m_undoButton;
    GradientButton *m_redoButton;
    GradientButton *m_boldButton;
    GradientButton *m_italicButton;
    GradientButton *m_underlineButton;
    QComboBox *m_comboSize;

    // Block format style options
    GradientButton *m_headingLevel1Button;
    GradientButton *m_headingLevel2Button;
    GradientButton *m_headingLevel3Button;
    GradientButton *m_headingLevel4Button;
    GradientButton *m_paragraphButton;
    GradientButton *m_listButton;
    GradientButton *m_moreIndentButton;
    GradientButton *m_lessIndentButton;

    // Insertion
    GradientButton *m_linkButton;
    GradientButton *m_emojiButton;
};

#endif // TEXTEDITORWIDGET_H
