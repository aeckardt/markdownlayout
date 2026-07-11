#include <QApplication>
#include <QTimer>

#include "io/htmlparser.h"

#define USE_MARKDOWN_LAYOUT

static const char *htmlString =
        "<html>"
        "<head>"
        "</head>"
        "<body>"
        "<h1>A heading</h1>"
        "<p>A paragraph</p>"
        "</body>"
        "</html>";

void printAst(const HtmlParser &parser, const HtmlNodePtr &node, int level = 0)
{
    switch (node->type()) {
    case HtmlNode::Type::HtmlTag: {
        QByteArray indent(2 * level, ' ');
        printf("%s", qUtf8Printable(indent));
        printf("<%s>\n", qUtf8Printable(node->tag()->name()));
        break;
    }
    case HtmlNode::Type::Text: {
        QByteArray indent(2 * level, ' ');
        printf("%s", qUtf8Printable(indent));
        printf("%s\n", qUtf8Printable(parser.text(node)));
        break;
    }
    default:
        ;
    }

    for (const HtmlNodePtr &child : node->children()) {
        int nextLevel = level;
        if (node->type() != HtmlNode::Type::Container)
            ++nextLevel;
        printAst(parser, child, nextLevel);
    }

    switch (node->type()) {
    case HtmlNode::Type::HtmlTag: {
        QByteArray indent(2 * level, ' ');
        printf("%s", qUtf8Printable(indent));
        printf("</%s>\n", qUtf8Printable(node->tag()->name()));
        break;
    }
    default:
        ;
    }

}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    HtmlParser parser(htmlString);
    HtmlNodePtr ast = parser.parse();

    printAst(parser, ast);

    QTimer::singleShot(0, &app, QCoreApplication::quit);

    return app.exec();
}
