#include "mainwindow.h"

#include <QBoxLayout>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextList>

#include "qtextdocumentlayout/qtextdocumentlayout.h"
#include "markdownlayout/markdownlayout.h"

//#define DEFAULT_TEXTDOCUMENTLAYOUT

QString htmlInput1 =
        "<h1>Heading 1</h1>\n"
        "<p>Normal paragraph with <b>bold</b>, <i>italic</i>, <u>underline</u>, and <a href=\"https://example.com\">link</a>.</p>\n"
        "<h2>Heading 2</h2>\n"
        "<ul>\n"
          "<li>First item</li>\n"
          "<li>Second item with longer wrapped text that spans multiple lines.</li>\n"
        "</ul>\n"
        "<hr>\n"
        "<pre>code block line 1\n"
        "code block line 2\n"
        "very very very long code line...</pre>\n";

QString htmlInput2 =
        "<blockquote>\n"
          "<p>A quoted paragraph with wrapping.</p>\n"
        "</blockquote>\n"
        "\n"
        "<table border=\"1\">\n"
          "<tr><td>Cell A</td><td>Cell B</td></tr>\n"
          "<tr><td>Long wrapping cell content...</td><td>Cell D</td></tr>\n"
        "</table>\n"
        "\n"
        "<p>Text before image <img src=\"...\"> text after image.</p>\n";

QString makeLongHtml(int paragraphs)
{
    QString html;

    for (int i = 0; i < paragraphs; ++i) {
        if (i % 20 == 0)
            html += QString("<h2>Heading %1</h2>").arg(i);

        if (i % 7 == 0) {
            html += "<ul>";
            html += "<li>First list item with some longer wrapping text.</li>";
            html += "<li>Second list item with <b>bold</b> text.</li>";
            html += "</ul>";
        } else {
            html += QString(
                "<p>Paragraph %1 with <b>bold</b>, <i>italic</i>, "
                "some longer text that wraps across multiple lines, "
                "and a link-like section.</p>"
            ).arg(i);
        }
    }

    return html;
}

QFont defaultMonospaceFont()
{
    auto families = QFontDatabase::families();
    for (const QString &family : families) {
        if (family == "Menlo" || family == "Monaco" || family == "Consolas" || family == "Courier New") {
            return QFont(family, 13);
        }
    }

    QFont font("Monospace", 13);
    font.setStyleHint(QFont::StyleHint::Monospace);
    return font;
}

void setBlockType(QTextCursor *cursor, int type)
{
    QTextBlockFormat blockFormat(cursor->blockFormat());
    if (type == BLOCK_TYPE_NORMAL) {
        blockFormat.clearProperty(BLOCK_TYPE_PROPERTY);
    } else {
        blockFormat.setProperty(BLOCK_TYPE_PROPERTY, type);
    }
    cursor->setBlockFormat(blockFormat);

    if (type == BLOCK_TYPE_CODE) {
        QTextCharFormat charFormat;
        charFormat.setFont(defaultMonospaceFont());
        cursor->select(QTextCursor::SelectionType::BlockUnderCursor);
        cursor->mergeCharFormat(charFormat);
        cursor->clearSelection();
    }
}

void buildSampleDocument(QTextDocument *document)
{
    QTextCursor cursor(document);
    cursor.beginEditBlock();

    QTextCharFormat normalFormat(cursor.charFormat());
    QTextCharFormat headingFormat(cursor.charFormat());
    headingFormat.setFontWeight(QFont::Weight::DemiBold);
    headingFormat.setFontPointSize(21);
    cursor.insertText("MarkdownLayout Demo", headingFormat);
    cursor.setCharFormat(normalFormat);

    cursor.insertBlock();
    cursor.insertText(
        "This document is still a QTextDocument. The visible layout, however, "
        "is drawn by a custom QAbstractTextDocumentLayout subclass. Try typing, "
        "selecting text, resizing the window, and using the toolbar."
    );

    cursor.insertBlock();
    setBlockType(&cursor, BLOCK_TYPE_QUOTE);
    cursor.insertText(
        "This block has quote metadata stored in its QTextBlockFormat. "
        "The gray bar and background are painted by the layout, not by text characters."
    );

    cursor.insertBlock();
    setBlockType(&cursor, BLOCK_TYPE_NORMAL);
    cursor.insertText("The next two blocks are real QTextList blocks, but the layout draws Markdown-style dashes:");

    cursor.insertBlock();
    cursor.insertText("First list item with a painted dash marker");
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::Style::ListDisc);
    listFormat.setIndent(1);
    cursor.createList(listFormat);

    cursor.insertBlock();
    cursor.insertText("Second list item; the dash is not a selectable character");

    // End the list for the following block.
    QTextList *currentList = cursor.currentList();
    cursor.insertBlock();
    if (currentList) {
        currentList->remove(cursor.block());
    }

    setBlockType(&cursor, BLOCK_TYPE_CODE);
    QTextCharFormat codeCharFormat;
    codeCharFormat.setFont(defaultMonospaceFont());
    cursor.insertText(
        "class Example:\n"
        "    def paint(self, painter):\n"
        "        painter.drawText(0, 0, 'custom layout')",
        codeCharFormat
    );

    cursor.insertBlock();
    setBlockType(&cursor, BLOCK_TYPE_NORMAL);
    cursor.insertText(
        "This is intentionally not production-ready. The value of the spike is "
        "that block positions, hit testing, cursor drawing and visual decoration "
        "are now all visible in one small file."
    );

    cursor.endEditBlock();
}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    QTextEdit *textEdit = new QTextEdit();

    QTextDocument *document = textEdit->document();
#ifndef DEFAULT_TEXTDOCUMENTLAYOUT
    QAbstractTextDocumentLayout *customLayout = new MarkdownLayout(document);
    document->setDocumentLayout(customLayout);
#endif
    //document->setHtml(htmlInput1);
    buildSampleDocument(document);
    textEdit->setDocument(document);

    setCentralWidget(textEdit);

    resize(720, 500);
    setWindowTitle("QTextDocumentLayout Demo");
}
