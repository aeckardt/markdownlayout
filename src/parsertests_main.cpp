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

static const char *markdownString =
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

    // Round trip test
    QTextDocument *document1 = documentFromMarkdown(markdownString, &app);
    QString exportedHtml = HtmlExporter::exportDocument(document1);
    QTextDocument *document2 = documentFromHtml(exportedHtml, &app);
    QString exportedMd = MarkdownExporter::exportDocument(document2);

    // Is the original data preserved even with double conversion?
    Q_ASSERT(document1->toHtml() == document2->toHtml());
    Q_ASSERT(markdownString == exportedMd);

    // Show created document in window
    MainWindow wnd;
    wnd.setDocument(document2);
    wnd.show();

    return app.exec();
}
