#include <QApplication>
#include <QBoxLayout>
#include <QMainWindow>
#include <QTimer>
#include <QTextEdit>
#include <QTextDocument>

#include "io/htmlimporter.h"
#include "io/htmlwriter.h"
#include "io/markdownimporter.h"
#include "io/markdownwriter.h"

static const char *markdownString =
        "# 1. Überschrift\n"
        "* Listenpunkt 1\n"
        "* Listenpunkt 2\n"
        "## 2. Überschrift\n"
        "Etwas **fetter** und etwas *kursiver* Text.";

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
    QTextDocument *doc1 = documentFromMarkdown(markdownString, &app);
    QByteArray exportedHtml = htmlFromDocument(doc1).toUtf8();
    QTextDocument *doc2 = documentFromHtml(exportedHtml, &app);
    QByteArray exportedMd = markdownFromDocument(doc2);

    // Is the original data preserved even after double conversion?
    Q_ASSERT(doc1->toHtml() == doc2->toHtml());
    Q_ASSERT(markdownString == exportedMd);

    // Show created document in window
    MainWindow wnd;
    wnd.setDocument(doc2);
    wnd.show();

    return app.exec();
}
