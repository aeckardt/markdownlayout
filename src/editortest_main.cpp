#include <QApplication>
#include <QBoxLayout>
#include <QMainWindow>
#include <QTimer>
#include <QTextEdit>
#include <QTextDocument>

#include "serialization/htmlimporter.h"
#include "serialization/htmlwriter.h"
#include "serialization/markdownimporter.h"
#include "serialization/markdownwriter.h"
#include "texteditor/texteditor.h"
#include "texteditor/texteditorwidget.h"

static const char *markdownString =
        "# 1. Überschrift\n"
        "* Listenpunkt 1\n"
        "* Listenpunkt 2\n"
        "\n"
        "## 2. Überschrift\n"
        "Etwas **fetter** und etwas *kursiver* Text.\n"
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
        setWindowTitle("Markdown Editor Test");
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

    // Show created document in window
    MainWindow wnd;
    wnd.setDocument(document);
    wnd.show();

    return app.exec();
}
