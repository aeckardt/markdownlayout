#include "markdownwriter.h"

#include "inlineformatresolver.h"
#include "textformat/constdefs.h"

#include <QByteArray>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextList>
#include <QUrl>

struct ExportableFragment;

class MarkdownWriter
{
public:
    MarkdownWriter(QTextDocument *document, const QTextCursor *range);
    QByteArray exportAll();

private:
    QByteArray exportBlock(const QTextBlock &block);
    QPair<QByteArray, int> exportFragment(const ExportableFragment &fragment,
                                          int headingLevel = 0);

    QTextDocument *m_document;
    int m_start;
    int m_end;
};

MarkdownWriter::MarkdownWriter(QTextDocument *document, const QTextCursor *range)
    : m_document(document)
{
    if (!range) {
        m_start = 0;
        m_end = document->characterCount();
    } else {
        m_start = range->selectionStart();
        m_end = range->selectionEnd();
    }
}

QByteArray MarkdownWriter::exportAll()
{
    if (!m_document || m_end < m_start)
        return {};

    QByteArrayList lines;
    QTextCursor localCursor(m_document);
    localCursor.setPosition(m_start);
    QTextBlock block = localCursor.block();

    while (block.isValid() && block.position() < m_end) {
        lines << exportBlock(block);
        block = block.next();
    }

    return lines.join('\n');
}

QByteArray MarkdownWriter::exportBlock(const QTextBlock &block)
{
    const QTextBlockFormat blockFormat = block.blockFormat();
    const int headingLevel = blockFormat.headingLevel();
    QTextList *textList = block.textList();

    if (blockFormat.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
        return "---";

    QByteArray prefix;
    if (textList)
        prefix = QByteArray(blockFormat.indent() * 2, ' ') + "* ";
    else if (headingLevel >= 1 && headingLevel <= 4)
        prefix = QByteArray(headingLevel, '#') + ' ';
    else if (blockFormat.hasProperty(QTextFormat::BlockQuoteLevel))
        prefix = "> ";

    const auto fragments = InlineFormatResolver(block, m_start, m_end).fragments();
    QByteArray lineText;

    for (const auto &ef : fragments) {
        if (!ef.fragment.isValid())
            break;

        const auto exported = exportFragment(ef, headingLevel);
        lineText += exported.first;
        if (exported.second <= 0)
            break;
    }

    return prefix + lineText;
}

QPair<QByteArray, int> MarkdownWriter::exportFragment(const ExportableFragment &ef,
                                                     int headingLevel)
{
    const auto fragment = ef.fragment;
    QByteArray text = fragment.text().toUtf8();

    const int sliceLeft = std::max(0, m_start - fragment.position());
    const int remaining = m_end - fragment.position() - fragment.length();
    int sliceRight = text.size();
    if (remaining < 0)
        sliceRight = std::max<int>(0, text.size() + remaining);

    QByteArray selectedText = text.mid(sliceLeft, sliceRight - sliceLeft);

    static constexpr QByteArrayView whitespaceChars(" \n\t\r");

    qsizetype n;
    QByteArray leadingWs;
    for (n = 0; n < selectedText.size(); ++n) {
        if (!whitespaceChars.contains(selectedText[n]))
            break;
    }
    leadingWs = selectedText.first(n);
    if (leadingWs.size() == selectedText.size())
        return { leadingWs, remaining };
    QByteArray trailingWs;
    for (n = selectedText.size() - 1; n >= 0; --n) {
        if (!whitespaceChars.contains(selectedText[n]))
            break;
    }
    trailingWs = selectedText.last(selectedText.size() - n - 1);

    const QByteArray core = selectedText.mid(leadingWs.size(), selectedText.size() - leadingWs.size() - trailingWs.size());

    QByteArrayList opening;
    QByteArrayList closing;
    for (const auto &change : ef.formatChanges) {
        switch (change.type) {
        case InlineFormat::Bold:
            (change.open ? opening : closing) << "**";
            break;
        case InlineFormat::Italic:
            (change.open ? opening : closing) << "*";
            break;
        case InlineFormat::Underline:
            (change.open ? opening : closing) << (change.open ? "<ins>" : "</ins>");
            break;
        case InlineFormat::PointSize:
            if (headingLevel > 0)
                // Heading size is exclusively represented by the heading block type.
                break;
            if (change.open)
                opening << "<span style=\"font-size:" << change.attrs.value("font-size") << "pt\">";
            else
                closing << "</span>";
            break;
        case InlineFormat::Link:
            if (change.open) {
                opening << "[";
            } else {
                const QByteArray encoded = QUrl::toPercentEncoding(change.attrs.value("href"), ":/");
                closing << "](" << encoded << ")";
            }
            break;
        }
    }

    return {leadingWs + opening.join(QByteArray()) + core + closing.join(QByteArray()) + trailingWs, remaining};
}

QByteArray markdownFromDocument(QTextDocument *document, const QTextCursor *range)
{
    return MarkdownWriter(document, range).exportAll();
}
