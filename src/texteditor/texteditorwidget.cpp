#include "texteditorwidget.h"

#include "gradientbutton.h"
#include "texteditor.h"
#include "texteditorstyle.h"
#include "toolbarseparator.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIntValidator>
#include <QList>
#include <QToolBar>

using namespace Qt::StringLiterals;
using namespace TextEditorStyle;

TextEditorWidget::TextEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    // Setup layout with toolbar and textedit
    QBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);

    // Initialize toolbar widget
    m_toolbar = new QToolBar;
    m_toolbar->setIconSize({24, 26});
    m_toolbar->setStyleSheet("QToolBar { spacing: 0px; }"_L1);
    layout->addWidget(m_toolbar);

    // Initialize TextEditor
    m_textEdit = new TextEditor(this);
    connect(m_textEdit, &TextEditor::fontChanged, this, &TextEditorWidget::onFontChanged);
    connect(m_textEdit, &TextEditor::blockFormatChanged, this, &TextEditorWidget::onBlockFormatChanged);
    layout->addWidget(m_textEdit);

    // Setup toolbuttons
    // This happens after m_textedit is initialized, because it uses its actions, slots and other methods
    setupToolBar();

    setLayout(layout);
}

void TextEditorWidget::setupToolBar()
{
    addSeparator();

    // Undo / Redo
    m_undoButton = addToolButton("undo"_L1, m_textEdit->m_undoAction);
    m_redoButton = addToolButton("redo"_L1, m_textEdit->m_redoAction);

    addSeparator();

    // Char format styles
    m_boldButton = addToolButton("bold"_L1, m_textEdit->m_boldAction);
    m_italicButton = addToolButton("italic"_L1, m_textEdit->m_italicAction);
    m_underlineButton = addToolButton("underline"_L1, m_textEdit->m_underlineAction);

    addSeparator();

    // Text size ComboBox
    m_comboSize = new QComboBox;
    m_comboSize->setObjectName("comboSize");
    m_comboSize->setMinimumWidth(80);
    m_comboSize->setEditable(true);
    m_comboSize->setValidator(new QIntValidator);
    m_toolbar->addWidget(m_comboSize);

    QList<int> standardSizes = QFontDatabase::standardSizes();
    for (int size : standardSizes) {
        if (size > 72)
            break;
        m_comboSize->addItem(QString::number(size));
    }
    int defaultIndex = standardSizes.indexOf(defaultFontPointSize());
    if (defaultIndex != -1)
        m_comboSize->setCurrentIndex(defaultIndex);
    connect(m_comboSize, &QComboBox::textActivated, m_textEdit, &TextEditor::setTextSize);

    addSeparator();

    // Block styles
    m_headingLevel1Button = addExtToolButton("heading1"_L1,
                                             [this]() { setHeadingLevel(1); },
                                             true, true, tr("Heading Level 1"));
    m_headingLevel2Button = addExtToolButton("heading2"_L1,
                                             [this]() { setHeadingLevel(2); },
                                             true, true, tr("Heading Level 2"));
    m_headingLevel3Button = addExtToolButton("heading3"_L1,
                                             [this]() { setHeadingLevel(3); },
                                             true, true, tr("Heading Level 3"));
    m_headingLevel4Button = addExtToolButton("heading4"_L1,
                                             [this]() { setHeadingLevel(4); },
                                             true, true, tr("Heading Level 4"));
    m_paragraphButton = addExtToolButton("paragraph"_L1,
                                         std::bind(&TextEditor::removeBlockStyle, m_textEdit),
                                         true, false, tr("Paragraph"));
    m_listButton = addExtToolButton("list_ul"_L1,
                                    std::bind(&TextEditor::toggleList, m_textEdit),
                                    true, true, tr("List"));

    addSeparator();

    // Indent
    m_moreIndentButton = addToolButton("indent"_L1, m_textEdit->m_moreIndentAction);
    m_lessIndentButton = addToolButton("indent-flipped", m_textEdit->m_lessIndentAction);

    addSeparator();

    // Insert link / emoji
    m_linkButton = addExtToolButton("link"_L1,
                                    std::bind(&TextEditor::insertHyperlink, m_textEdit),
                                    true, false, tr("Insert link..."));
    m_emojiButton = addToolButton("face-grin"_L1, m_textEdit->m_insertEmojiAction);

    addSeparator();
}

GradientButton *TextEditorWidget::addToolButton(const QString &iconName, QAction *action)
{
    GradientButton *btn = new GradientButton(loadIconImage(iconName), false,
                                             QColor("#ebebeb"), QColor("#c7c7c7"));

    if (!action->isEnabled())
        btn->setEnabled(false);
    if (action->isCheckable())
        btn->setCheckable(true);

    connect(action, &QAction::enabledChanged, btn, &GradientButton::setEnabled);
    connect(action, &QAction::toggled, btn, &GradientButton::setChecked);
    connect(btn, &GradientButton::clicked, action, &QAction::trigger);

    if (!action->text().isEmpty()) {
        QString tooltip = action->text();
        if (!action->shortcut().toString().isEmpty())
            tooltip += QStringLiteral(" (%1)").arg(action->shortcut().toString());
        btn->setToolTip(tooltip);
    }

    m_toolbar->addWidget(btn);

    return btn;
}

GradientButton *TextEditorWidget::addExtToolButton(const QString &iconName, const ClickHandler &clickFnc,
                                                   bool enabled, bool checkable,
                                                   const QString &tooltip)
{
    GradientButton *btn = new GradientButton(loadIconImage(iconName), false,
                                             QColor("#ebebeb"), QColor("#c7c7c7"));

    if (!enabled)
        btn->setEnabled(false);
    if (checkable)
        btn->setCheckable(true);
    if (clickFnc)
        connect(btn, &GradientButton::clicked, [clickFnc]() { clickFnc(); } );
    if (!tooltip.isEmpty())
        btn->setToolTip(tooltip);

    m_toolbar->addWidget(btn);

    return btn;
}

void TextEditorWidget::addSeparator()
{
    m_toolbar->addWidget(new ToolBarSeparator());
}

void TextEditorWidget::onFontChanged(const QFont &font)
{
    const QString sizeStr = QString::number(font.pointSize());
    m_comboSize->setCurrentIndex(m_comboSize->findText(sizeStr));
    m_comboSize->setEditText(sizeStr);
}

void TextEditorWidget::onBlockFormatChanged()
{
    QTextCursor cursor = m_textEdit->textCursor();
    const QTextBlockFormat blockFmt = cursor.blockFormat();
    int headingLevel = blockFmt.headingLevel();
    bool textList = cursor.currentList() != nullptr;

    m_headingLevel1Button->setChecked(headingLevel == 1);
    m_headingLevel2Button->setChecked(headingLevel == 2);
    m_headingLevel3Button->setChecked(headingLevel == 3);
    m_headingLevel4Button->setChecked(headingLevel == 4);
    m_listButton->setChecked(textList);
}

void TextEditorWidget::setHeadingLevel(int level)
{
    int currentLevel = m_textEdit->textCursor().blockFormat().headingLevel();
    if (currentLevel != level)
        m_textEdit->setHeadingLevel(level);
    else {
        // The button is automatically unchecked after being clicked
        // That's why it's necessary to set it to checked again
        // as long as the blockFormat has the according heading level
        switch (level) {
        case 1:
            m_headingLevel1Button->setChecked(true);
            break;
        case 2:
            m_headingLevel2Button->setChecked(true);
            break;
        case 3:
            m_headingLevel3Button->setChecked(true);
            break;
        case 4:
            m_headingLevel4Button->setChecked(true);
            break;
        default:
            ;
        }
    }
}

QImage TextEditorWidget::loadIconImage(const QString &iconName)
{
    QString filePath = QStringLiteral("icons/fontawesome/%1-24.png").arg(iconName);

    if (!QFileInfo::exists(filePath))
        filePath = QStringLiteral("icons/%1-24.png").arg(iconName);

    return QImage(filePath, "PNG");
}
