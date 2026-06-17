#ifndef TEXTEDITOR_P_H
#define TEXTEDITOR_P_H

#include <QMetaType>
#include <memory>

class QString;
class QTextBlock;
class QTextCharFormat;
class QTextCursor;

struct Hyperlink {
    int position;
    int length;
    const QString text;
    const QString href;

    Hyperlink(int position, int length, const QString &text, const QString &href)
        : position(position), length(length), text(text), href(href)
    {}
};

typedef std::shared_ptr<Hyperlink> HyperlinkPtr;
Q_DECLARE_METATYPE(HyperlinkPtr)

void systemOpenFile(const QString &filename, const QString &dirPath = {});

#endif // TEXTEDITOR_P_H
