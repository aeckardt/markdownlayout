#include "texteditor/htmlexporter.h"
#include "texteditor/inlineformatresolver.h"
#include "texteditor/texteditorstyle.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextList>

using namespace Qt::StringLiterals;

QString HtmlExporter::exportDocument(QTextDocument *document, const QTextCursor *range, bool skipHeader)
{
    HtmlExporter exporter(document, range, skipHeader);
    return exporter.exportAll();
}

HtmlExporter::HtmlExporter(QTextDocument *document, const QTextCursor *range, bool skipHeader)
    : m_document(document), m_skipHeader(skipHeader)
{
    Q_ASSERT(document);
    if (!range) {
        m_start = 0;
        const QTextBlock last = document->lastBlock();
        m_end = last.isValid() ? last.position() + last.length() : -1;
    } else {
        m_start = range->selectionStart();
        m_end = range->selectionEnd();
    }
}

QString HtmlExporter::exportAll()
{
    if (m_end < m_start)
        return "";

    QString htmlOutput;
    if (!m_skipHeader)
        // Create the basic HTML header and style definitions.
        htmlOutput += htmlHeader();

    // Set block to where the range starts
    QTextCursor localCursor(m_document);
    localCursor.setPosition(m_start);
    QTextBlock block = localCursor.block();

    while (block.isValid() && block.position() <= m_end) {
        // Export the block (generate HTML for the block)
        htmlOutput += exportBlock(block) + QLatin1Char('\n');

        // Advance to the next block
        block = block.next();
    }

    if (!m_skipHeader)
        // Add footer
        htmlOutput += htmlFooter();
    else if (htmlOutput.endsWith(QLatin1Char('\n')))
        // Remove last newline
        htmlOutput.chop(1);

    return htmlOutput;
}

QString HtmlExporter::exportBlock(const QTextBlock &block)
{
    // Open the block (generate starting HTML for the block)
    QString lineHtml = blockFormatToHtml(block, true);

    // Resolve char formats
    const auto fragments = InlineFormatResolver(block, m_start, m_end).fragments();

    // Process each fragment in the block
    for (const auto &ef : fragments) {
        const auto fragment = ef.fragment;
        if (!fragment.isValid())
            break;

        QString text = fragment.text();

        // Calculate selection boundaries within this fragment
        const int sliceLeft = std::max(0, m_start - fragment.position());
        const int remaining = m_end - fragment.position() - fragment.length();
        int sliceRight = text.size();
        if (remaining < 0)
            sliceRight = std::max<int>(0, text.size() + remaining);
        const QString selectedText = text.mid(sliceLeft, sliceRight - sliceLeft);

        // For non-heading blocks, wrap each fragment with its char format.
        if (block.blockFormat().headingLevel() == 0)
            lineHtml += inlineFormatToHtml(ef, true);

        // Add selected text within fragment
        lineHtml += selectedText.toHtmlEscaped();

        // For non-heading blocks, close formatting tags
        if (block.blockFormat().headingLevel() == 0)
            lineHtml += inlineFormatToHtml(ef, false);

        if (remaining <= 0)
            break;
    }

    // Close the block's formatting
    lineHtml += blockFormatToHtml(block, false);
    return lineHtml;
}

/*
 * Build HTML for a block (paragraph, heading, or list item).
 * Uses the open_tags stack to track which tags are open.
 * Uses inline CSS to represent indent via margin-left.
*/
QString HtmlExporter::blockFormatToHtml(const QTextBlock &block, bool open)
{
    const QTextBlockFormat blockFormat = block.blockFormat();
    const int indent = blockFormat.indent();
    const QString indentStyle = indent > 0 ? QStringLiteral(" style=\"-qt-block-indent: %1;\"").arg(indent) : QString();

    int nestedUlTags = 0;
    for (const auto &tag : m_openTags) {
        if (tag.name == "ul"_L1)
            ++nestedUlTags;
    }

    // When opening a block, add the appropriate tag and push it to open_tags.
    if (open) {
        // Handle horizontal ruler
        if (blockFormat.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
            return "<hr width=\"60%\"/>"_L1;

        // Handle heading (if headingLevel > 0)
        if (blockFormat.headingLevel() > 0) {
            const QString tag = QStringLiteral("h%1").arg(blockFormat.headingLevel());
            m_openTags.append(Tag{tag});
            return QStringLiteral("<%1%2>").arg(tag, indentStyle);
        }

        // Handle list item
        if (block.textList()) {
            QString output;

            // Determine how many nested <ul> tags are needed.
            // Convention: we assume that each block's indent corresponds to (indent_level + 1) levels.
            while (nestedUlTags < indent + 1) {
                m_openTags.append({"ul"_L1});
                output += "<ul>"_L1;
                ++nestedUlTags;
            }

            // Now, open the list item.
            m_openTags.append({"li"_L1});
            output += "<li>"_L1;

            return output;
        }

        // Otherwise, treat as a regular paragraph.
        m_openTags.append({"p"_L1});
        return QStringLiteral("<p%1>").arg(indentStyle);
    }

    // Closing tags for this block.
    QString output;

    // If a list item was opened, close it.
    if (!m_openTags.isEmpty() && m_openTags.last().name == "li"_L1) {
        output += "</li>"_L1;
        m_openTags.removeLast();
    }

    // For list blocks, determine if subsequent blocks are part of the same list.
    if (block.textList()) {
        // Look ahead to the next block; if the next block is a list,
        // adjust the number of open <ul> tags according to its indent.
        QTextBlock nextBlock = block.next();
        if (nextBlock.isValid() && nextBlock.position() < m_end && nextBlock.textList()) {
            const int nextIndent = nextBlock.blockFormat().indent();

            // Close extra <ul> tags if the current indent is higher.
            while (nestedUlTags > nextIndent + 1) {
                output += "</ul>"_L1;
                --nestedUlTags;

                // Remove the last occurrence of 'ul' from open_tags.
                for (int i = m_openTags.size() - 1; i >= 0; --i) {
                    if (m_openTags[i].name == "ul"_L1) {
                        m_openTags.removeAt(i);
                        break;
                    }
                }
            }

            // Do not close if they match.
        } else {
            // If the next block is not a list, close all open <ul> tags.
            for (int i = m_openTags.size() - 1; i >= 0; --i) {
                if (m_openTags[i].name == "ul"_L1) {
                    output += "</ul>"_L1;
                    m_openTags.removeAt(i);
                }
            }
        }
        return output;
    }

    // For paragraphs or headings, simply close all remaining block-level tags.
    while (!m_openTags.isEmpty() && m_openTags.last().name != "ul"_L1 && m_openTags.last().name != "li"_L1) {
        const auto tag = m_openTags.takeLast();
        output += QStringLiteral("</%1>").arg(tag.name);
    }
    return output;
}

QString HtmlExporter::inlineFormatToHtml(const ExportableFragment &fragment, bool open) const
{
    QStringList tags;
    for (const auto &change : fragment.formatChanges) {
        if (open != change.open)
            continue;
        switch (change.type) {
        case InlineFormat::Type::Link:
            tags << (open ? QStringLiteral("<a href=\"%1\">").arg(change.attrs.value("href"_L1).toHtmlEscaped()) : "</a>"_L1);
            break;
        case InlineFormat::Type::PointSize:
            tags << (open ? QStringLiteral("<span style=\"font-size:%1pt\">").arg(change.attrs.value("font-size"_L1)) : "</span>"_L1);
            break;
        case InlineFormat::Type::Bold:
            tags << (open ? "<strong>"_L1 : "</strong>"_L1);
            break;
        case InlineFormat::Type::Italic:
            tags << (open ? "<em>"_L1 : "</em>"_L1);
            break;
        case InlineFormat::Type::Underline:
            tags << (open ? "<ins>"_L1 : "</ins>"_L1);
            break;
        }
    }
    return tags.join(QString());
}

QString HtmlExporter::htmlHeader()
{
    return QStringLiteral(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
        "<style type=\"text/css\">\n"
        "p, li { white-space: pre-wrap; }\n"
        "p, li, h1, h2, h3, h4, ul { line-height: 125%; }\n"
        "p, li, h1, h2, h3, h4 { margin-top: 0px; margin-bottom: 2px; }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n");
}

QString HtmlExporter::htmlFooter()
{
    return QStringLiteral("</body>\n</html>");
}
