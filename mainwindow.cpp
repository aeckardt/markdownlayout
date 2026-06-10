#include "mainwindow.h"

#include <QBoxLayout>
#include <QTextEdit>
#include <QTextDocument>

#include "qtextdocumentlayout/qtextdocumentlayout.h"
#include "markdownlayout.h"

//#define DEFAULT_TEXTDOCUMENTLAYOUT

QString htmlInput1 =
        "<h1>Heading 1</h1>\n"
        "<p>Normal paragraph with <b>bold</b>, <i>italic</i>, <u>underline</u>, and <a href=\"https://example.com\">link</a>.</p>\n"
        "<h2>Heading 2</h2>\n"
        "<ul>\n"
        "  <li>First item</li>\n"
        "  <li>Second item with longer wrapped text that spans multiple lines.</li>\n"
        "</ul>\n"
        "<hr>\n"
        "<pre>code block line 1\n"
        "code block line 2\n"
        "very very very long code line...</pre>\n";

QString htmlInput2 =
        "<blockquote>\n"
        "  <p>A quoted paragraph with wrapping.</p>\n"
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

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    QTextEdit *textEdit = new QTextEdit();

    QTextDocument *document = textEdit->document();
#ifndef DEFAULT_TEXTDOCUMENTLAYOUT
    QAbstractTextDocumentLayout *customLayout = new MarkdownLayout(document);
    document->setDocumentLayout(customLayout);
#endif
    document->setHtml(htmlInput1);
    textEdit->setDocument(document);

    setCentralWidget(textEdit);

    setWindowTitle("QTextDocumentLayout Demo");
}
