#include "texteditor/markdownexporter.h"
#include "texteditor/inlineformatresolver.h"
#include "texteditor/texteditorstyle.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextList>
#include <QUrl>

QString MarkdownExporter::exportDocument(QTextDocument *document, const QTextCursor *range)
{
    return MarkdownExporter(document, range).exportAll();
}

MarkdownExporter::MarkdownExporter(QTextDocument *document, const QTextCursor *range)
    : m_document(document)
{
    if (!document)
        return;

    if (!range) {
        m_start = 0;
        const QTextBlock last = document->lastBlock();
        m_end = last.isValid() ? last.position() + last.length() : -1;
    } else {
        m_start = range->selectionStart();
        m_end = range->selectionEnd();
    }
}

QString MarkdownExporter::exportAll()
{
    if (!m_document || m_end < m_start)
        return {};

    QStringList lines;
    QTextCursor localCursor(m_document);
    localCursor.setPosition(m_start);
    QTextBlock block = localCursor.block();

    while (block.isValid() && block.position() < m_end) {
        lines << exportBlock(block);
        block = block.next();
    }
    return lines.join(QLatin1Char('\n'));
}

QString MarkdownExporter::exportBlock(const QTextBlock &block)
{
    const QTextBlockFormat blockFormat = block.blockFormat();
    const int headingLevel = blockFormat.headingLevel();
    QTextList *textList = block.textList();

    if (blockFormat.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
        return QStringLiteral("---");

    QString prefix;
    if (textList)
        prefix = QString(blockFormat.indent() * 2, QLatin1Char(' ')) + QStringLiteral("* ");
    else if (headingLevel >= 1 && headingLevel <= 4)
        prefix = QString(headingLevel, QLatin1Char('#')) + QLatin1Char(' ');
    else if (blockFormat.hasProperty(QTextFormat::BlockQuoteLevel))
        prefix = QStringLiteral("> ");

    const auto fragments = InlineFormatResolver(block, m_start, m_end).fragments();
    QString lineText;

    for (const auto &fragment : fragments) {
        if (!fragment.fragment.isValid())
            break;

        const auto exported = exportFragment(fragment, headingLevel);
        lineText += exported.first;
        if (exported.second <= 0)
            break;
    }

    return prefix + lineText;
}

QPair<QString, int> MarkdownExporter::exportFragment(const ExportableFragment &ef,
                                                     int headingLevel)
{
    const auto fragment = ef.fragment;
    QString text = fragment.text();

    const int sliceLeft = std::max(0, m_start - fragment.position());
    const int remaining = m_end - fragment.position() - fragment.length();
    int sliceRight = text.size();
    if (remaining < 0)
        sliceRight = std::max<int>(0, text.size() + remaining);

    QString selectedText = text.mid(sliceLeft, sliceRight - sliceLeft);
    static const QRegularExpression leadingRe(QStringLiteral("^\\s*"));
    static const QRegularExpression trailingRe(QStringLiteral("\\s*$"));

    const QString leading = leadingRe.match(selectedText).captured(0);
    if (leading.size() == selectedText.size())
        return { leading, remaining };

    const QString trailing = trailingRe.match(selectedText).captured(0);
    const QString core = selectedText.mid(leading.size(), selectedText.size() - leading.size() - trailing.size());

    QStringList opening;
    QStringList closing;
    for (const auto &change : ef.formatChanges) {
        switch (change.type) {
        case InlineFormat::Bold:
            (change.open ? opening : closing) << QStringLiteral("**");
            break;
        case InlineFormat::Italic:
            (change.open ? opening : closing) << QStringLiteral("*");
            break;
        case InlineFormat::Underline:
            (change.open ? opening : closing) << (change.open ? QStringLiteral("<ins>") : QStringLiteral("</ins>"));
            break;
        case InlineFormat::PointSize:
            if (headingLevel > 0)
                break;
            if (change.open)
                opening << QStringLiteral("<span style=\"font-size:%1pt\">").arg(change.attrs.value(QStringLiteral("font-size")));
            else
                closing << QStringLiteral("</span>");
            break;
        case InlineFormat::Link:
            if (change.open) {
                opening << QStringLiteral("[");
            } else {
                const QByteArray encoded = QUrl::toPercentEncoding(change.attrs.value(QStringLiteral("href")), ":/");
                closing << QStringLiteral("](%1)").arg(QString::fromUtf8(encoded));
            }
            break;
        }
    }

    return {leading + opening.join(QString()) + core + closing.join(QString()) + trailing, remaining};
}
