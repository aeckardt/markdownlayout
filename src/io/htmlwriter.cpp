#include "htmlwriter.h"

#include "htmlstyle.h"
#include "inlineformatresolver.h"
#include "textformat/constdefs.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QVector>
#include <QTextList>

class HtmlWriter
{
public:
    HtmlWriter(QTextDocument *document, const QTextCursor *range, bool skipHeader);
    QByteArray exportAll();

private:
    QByteArray exportBlock(const QTextBlock &block);
    QByteArray blockFormatToHtml(const QTextBlock &block, bool open);
    QByteArray inlineFormatToHtml(const struct ExportableFragment &fragment, bool open) const;

    static QByteArray htmlHeader();
    static QByteArray htmlFooter();

    struct Tag {
        QByteArray name;
        CssProperties attrs;

        inline Tag(const QByteArray &name, const CssProperties &attrs = {})
            : name(name), attrs(attrs)
        {}
    };

    QTextDocument *m_document;
    int m_start;
    int m_end;
    bool m_skipHeader;
    QVector<Tag> m_openTags;
};

HtmlWriter::HtmlWriter(QTextDocument *document, const QTextCursor *range, bool skipHeader)
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

QByteArray HtmlWriter::exportAll()
{
    if (m_end < m_start)
        return "";

    QByteArray htmlOutput;
    if (!m_skipHeader)
        // Create the basic HTML header and style definitions.
        htmlOutput += htmlHeader();

    // Set block to where the range starts
    QTextCursor localCursor(m_document);
    localCursor.setPosition(m_start);
    QTextBlock block = localCursor.block();

    while (block.isValid() && block.position() <= m_end) {
        // Export the block (generate HTML for the block)
        htmlOutput += exportBlock(block) + '\n';

        // Advance to the next block
        block = block.next();
    }

    if (!m_skipHeader)
        // Add footer
        htmlOutput += htmlFooter();
    else if (htmlOutput.endsWith('\n'))
        // Remove last newline
        htmlOutput.chop(1);

    return htmlOutput;
}

QByteArray HtmlWriter::exportBlock(const QTextBlock &block)
{
    // Open the block (generate starting HTML for the block)
    QByteArray lineHtml = blockFormatToHtml(block, true);

    // Resolve char formats
    auto fragments = InlineFormatResolver(block, m_start, m_end).fragments();

    // Process each fragment in the block
    for (auto &ef : fragments) {
        const auto fragment = ef.fragment;
        if (!fragment.isValid())
            break;

        if (block.blockFormat().headingLevel() > 0) {
            // The pointsize of headings is exclusively defined by their heading level.
            // Therefore pointsize changes in headings are removed.
            for (int i = ef.formatChanges.count() - 1; i >= 0; --i) {
                const FormatChange &fc = ef.formatChanges.at(i);
                if (fc.type == InlineFormat::PointSize)
                    ef.formatChanges.removeAt(i);
            }
        }

        QString text = fragment.text();

        // Calculate selection boundaries within this fragment
        const int sliceLeft = std::max(0, m_start - fragment.position());
        const int remaining = m_end - fragment.position() - fragment.length();
        int sliceRight = text.size();
        if (remaining < 0)
            sliceRight = std::max<int>(0, text.size() + remaining);
        const QString selectedText = text.mid(sliceLeft, sliceRight - sliceLeft);

        // Wrap each fragment with its char format.
        lineHtml += inlineFormatToHtml(ef, true);

        // Add selected text within fragment
        lineHtml += selectedText.toHtmlEscaped().toUtf8();

        // Close formatting tags
        lineHtml += inlineFormatToHtml(ef, false);

        if (remaining <= 0)
            break;
    }

    // Close the block's formatting
    lineHtml += blockFormatToHtml(block, false);
    return lineHtml;
}

/* Build HTML for a block (paragraph, heading, or list item).
 * Uses the open_tags stack to track which tags are open.
 * Uses inline CSS to represent indent via margin-left. */
QByteArray HtmlWriter::blockFormatToHtml(const QTextBlock &block, bool open)
{
    const QTextBlockFormat blockFormat = block.blockFormat();
    const int indent = blockFormat.indent();
    QByteArray indentStyle;
    if (indent > 0)
        indentStyle = " style=\"-qt-block-indent: " + QByteArray::number(indent) + ";\"";

    int nestedUlTags = 0;
    for (const auto &tag : m_openTags) {
        if (tag.name == "ul")
            ++nestedUlTags;
    }

    // When opening a block, add the appropriate tag and push it to open_tags.
    if (open) {
        // Handle horizontal ruler
        if (blockFormat.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
            return "<hr width=\"60%\"/>";

        // Handle heading (if headingLevel > 0)
        if (blockFormat.headingLevel() > 0) {
            const QByteArray tag = "h" + QByteArray::number(blockFormat.headingLevel());
            m_openTags.append(Tag{tag});
            return "<" + tag + indentStyle + ">";
        }

        // Handle list item
        if (block.textList()) {
            QByteArray output;

            // Determine how many nested <ul> tags are needed.
            // Convention: we assume that each block's indent corresponds to (indent_level + 1) levels.
            while (nestedUlTags < indent + 1) {
                m_openTags.append({"ul"});
                output += "<ul>";
                ++nestedUlTags;
            }

            // Now, open the list item.
            m_openTags.append({"li"});
            output += "<li>";

            return output;
        }

        // Otherwise, treat as a regular paragraph.
        m_openTags.append({"p"});
        return "<p" + indentStyle + ">";
    }

    // Closing tags for this block.
    QByteArray output;

    // If a list item was opened, close it.
    if (!m_openTags.isEmpty() && m_openTags.last().name == "li") {
        output += "</li>";
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
                output += "</ul>";
                --nestedUlTags;

                // Remove the last occurrence of 'ul' from open_tags.
                for (int i = m_openTags.size() - 1; i >= 0; --i) {
                    if (m_openTags[i].name == "ul") {
                        m_openTags.removeAt(i);
                        break;
                    }
                }
            }

            // Do not close if they match.
        } else {
            // If the next block is not a list, close all open <ul> tags.
            for (int i = m_openTags.size() - 1; i >= 0; --i) {
                if (m_openTags[i].name == "ul") {
                    output += "</ul>";
                    m_openTags.removeAt(i);
                }
            }
        }
        return output;
    }

    // For paragraphs or headings, simply close all remaining block-level tags.
    while (!m_openTags.isEmpty()
           && m_openTags.last().name != "ul"
           && m_openTags.last().name != "li") {
        const auto tag = m_openTags.takeLast();
        output += "</" + tag.name + ">";
    }

    return output;
}

QByteArray HtmlWriter::inlineFormatToHtml(const ExportableFragment &fragment, bool open) const
{
    QByteArrayList tags;
    for (const auto &change : fragment.formatChanges) {
        if (open != change.open)
            continue;
        switch (change.type) {
        case InlineFormat::Link:
            tags << (open
                     ?"<a href=\"" + QString(change.attrs.value("href")).toHtmlEscaped().toUtf8() + ">"
                     : "</a>");
            break;
        case InlineFormat::PointSize:
            tags << (open
                     ? "<span style=\"font-size:" + change.attrs.value("font-size") + "pt\">"
                     : "</span>");
            break;
        case InlineFormat::Bold:
            tags << (open
                     ? "<strong>"
                     : "</strong>");
            break;
        case InlineFormat::Italic:
            tags << (open
                     ? "<em>"
                     : "</em>");
            break;
        case InlineFormat::Underline:
            tags << (open
                     ? "<ins>"
                     : "</ins>");
            break;
        }
    }
    return tags.join(QByteArray());
}

QByteArray HtmlWriter::htmlHeader()
{
    return "<!DOCTYPE html>\n"
           "<html>\n"
           "<head>\n"
           "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
           "<style type=\"text/css\">\n"
           "p, li { white-space: pre-wrap; }\n"
           "p, li, h1, h2, h3, h4, ul { line-height: 125%; }\n"
           "p, li, h1, h2, h3, h4 { margin-top: 0px; margin-bottom: 2px; }\n"
           "</style>\n"
           "</head>\n"
           "<body>\n";
}

QByteArray HtmlWriter::htmlFooter()
{
    return "</body>\n</html>";
}

QByteArray htmlFromDocument(QTextDocument *document, const QTextCursor *range, bool skipHeader)
{
    return HtmlWriter(document, range, skipHeader).exportAll();
}
