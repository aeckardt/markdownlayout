#include <QApplication>
#include <QBoxLayout>
#include <QMainWindow>
#include <QTimer>
#include <QTextEdit>
#include <QTextDocument>

#include "texteditor/htmlexporter.h"
#include "texteditor/htmlimporter.h"
#include "texteditor/markdownexporter.h"
#include "texteditor/markdownimporter.h"
#include "texteditor/markdownlayout.h"
#include "texteditor/texteditor.h"
#include "texteditor/texteditorwidget.h"

#define USE_MARKDOWN_LAYOUT

static const char *markdownString =
        "# 1. Überschrift\n"
        "* Listenpunkt 1\n"
        "* Listenpunkt 2\n"
        "\n"
        "## 2. Überschrift\n"
        "Etwas **fetter** und etwas *kursiver* Text.\n"
        "\n"
        "> Ein Zitat in einem BlockQuote.\n"
        "\n"
        "Und noch ein [Hyperlink](https://google.se).\n";

class MainWindow : public QMainWindow
{
public:
    MainWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags())
        : QMainWindow(parent, flags)
    {
        m_textEdit = new TextEditorWidget;
        setCentralWidget(m_textEdit);
        setWindowTitle("Markdown Layout Test");
        resize(750, 450);
    }

    void setDocument(QTextDocument *document)
    {
        m_textEdit->editor().setDocument(document);
    }

private:
    TextEditorWidget *m_textEdit;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // Create document from Markdown
    QTextDocument *document = documentFromMarkdown(markdownString, &app);

#ifdef USE_MARKDOWN_LAYOUT
    // Use custom layout system
    MarkdownLayout *layout = new MarkdownLayout(document);
    document->setDocumentLayout(layout);
#endif

    // Show created document in window
    MainWindow wnd;
    wnd.setDocument(document);
    wnd.show();

    return app.exec();
}
