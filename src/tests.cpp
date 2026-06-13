#include <QApplication>
#include <QMainWindow>
#include <QBoxLayout>
#include <QTextEdit>

#include <QTimer>
#include <QTextDocument>

#include "texteditor/htmlexporter.h"
#include "texteditor/htmlimporter.h"
#include "texteditor/markdownimporter.h"

static const char *MARKDOWN_STRING =
        "# 1. Überschrift\n"
        "* Listenpunkt 1\n"
        "* Listenpunkt 2\n"
        "\n"
        "## 2. Überschrift\n"
        "Etwas **fetter** und etwas *kursiver* Text.\n";

class MainWindow : public QMainWindow
{
public:
    MainWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags())
        : QMainWindow(parent, flags)
    {
        m_textEdit = new QTextEdit();
        setCentralWidget(m_textEdit);
        setWindowTitle("Markdown Test");
        resize(750, 450);
    }

    void setDocument(QTextDocument *document)
    {
        m_textEdit->setDocument(document);
    }

private:
    QTextEdit *m_textEdit;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QTextDocument document, newDocument;
    MarkdownImporter importer(&document);

    importer.import(QString(MARKDOWN_STRING));

    QString exported = HtmlExporter::exportDocument(&document);

    HtmlImporter htmlImporter(&newDocument);
    htmlImporter.import(exported);

    MainWindow wnd;
    wnd.setDocument(&newDocument);
    wnd.show();

    return app.exec();
}
