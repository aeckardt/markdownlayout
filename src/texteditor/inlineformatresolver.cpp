#include "texteditor/inlineformatresolver.h"
#include "texteditor/texteditorstyle.h"

#include <algorithm>

using namespace TextEditorStyle;

InlineFormatResolver::InlineFormatResolver(const QTextBlock &block, int start, int end)
    : m_block(block), m_start(start), m_end(end), m_useRange(start != -1 && end != -1), m_firstIndex(-1), m_lastIndex(-1)
{
    if (m_useRange)
        normalizeBoundaries();

    // Extract all format changes within this block
    detectFormatChanges();
    if (m_useRange)
        // Remove format changes that are not within the given range
        cleanUp();

    // Set up the format changes to be in the right order
    resolveChangeStack();
}

void InlineFormatResolver::resolveChangeStack()
{
    if (m_fragments.isEmpty())
        return;

    QVector<InlineFormat> openStack;

    // Set first and last index for iteration
    if (!m_useRange) {
        m_firstIndex = 0;
        m_lastIndex = m_fragments.size() - 1;
    }

    for (int index = m_firstIndex; index <= m_lastIndex; ++index) {
        // Resolve format changes for the current fragment
        auto &fragment = m_fragments[index - m_firstIndex];

        // Open formats at the beginning of the fragment
        QVector<InlineFormat> openFormats;
        for (const auto &f : m_formats) {
            if (f.start == index)
                openFormats.append(f);
        }
        std::sort(openFormats.begin(), openFormats.end(), [](const InlineFormat &a, const InlineFormat &b) {
            if (a.start != b.start)
                return a.start < b.start;
            if (a.end != b.end)
                return a.end > b.end;
            return static_cast<int>(a.type) < static_cast<int>(b.type);
        });

        // Add all opening formats
        for (const auto &fmt : openFormats) {
            fragment.formatChanges.append({fmt.type, fmt.attrs, true});
            openStack.append(fmt);
        }

        // Close formats at the end of the fragment
        QVector<InlineFormat> closeFormats;
        for (const auto &f : m_formats) {
            if (f.end == index)
                closeFormats.append(f);
        }
        std::sort(closeFormats.begin(), closeFormats.end(), [](const InlineFormat &a, const InlineFormat &b) {
            if (a.start != b.start)
                return a.start < b.start;
            return static_cast<int>(a.type) < static_cast<int>(b.type);
        });

        // Iterate through closing stack and append all closing formats
        while (!closeFormats.isEmpty()) {
            const InlineFormat closeFmt = closeFormats.takeLast();
            InlineFormat lastOpenFmt = openStack.takeLast();
            // Does the next closing format match the last opened format?
            while (lastOpenFmt.type != closeFmt.type) {
                // If they don't match, close the last opened format
                fragment.formatChanges.append({lastOpenFmt.type, lastOpenFmt.attrs, false});
                if (lastOpenFmt.end > index)
                    // Update start index of last opened format
                    // Thus, it will be re-opened in the next iteration
                    lastOpenFmt.start = index + 1;
                // TODO: (ChatGPT hint) Review the following code line
                // Verify it can be assumed that openStack is not empty
                // or do an assert
                lastOpenFmt = openStack.takeLast();
            }
            fragment.formatChanges.append({closeFmt.type, closeFmt.attrs, false});
        }
    }
}

void InlineFormatResolver::cleanUp()
{
    QVector<InlineFormat> updated;
    for (InlineFormat fmt : m_formats) {
        if (fmt.start <= m_lastIndex && fmt.end >= m_firstIndex) {
            if (fmt.start < m_firstIndex)
                fmt.start = m_firstIndex;
            if (fmt.end > m_lastIndex)
                fmt.end = m_lastIndex;
            updated.append(fmt);
        }
    }
    m_formats = updated;
}

void InlineFormatResolver::detectFormatChanges()
{
    QTextFragment previous;
    auto it = m_block.begin();
    int index = 0;

    while (!it.atEnd()) {
        // Char format from previous fragment
        const QTextCharFormat prevFmt = previous.isValid() ? previous.charFormat() : defaultCharFormat();

        // Char format from current fragment
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid())
            break;
        const QTextCharFormat curFmt = fragment.charFormat();

        // Char format from next fragment
        ++it;
        QTextCharFormat nextFmt = defaultCharFormat();
        if (!it.atEnd()) {
            const QTextFragment nextFragment = it.fragment();
            if (nextFragment.isValid())
                nextFmt = nextFragment.charFormat();
        }

        // Determine if fragment is within boundaries
        bool withinBoundaries = true;
        if (m_useRange) {
            withinBoundaries = m_start < fragment.position() + fragment.length() && m_end > fragment.position();
            if (withinBoundaries) {
                if (m_firstIndex == -1)
                    m_firstIndex = index;
                m_lastIndex = index;
            }
        }

        // Add fragment to list if that's the case
        if (withinBoundaries)
            m_fragments.append({fragment, {}});

        // Detect all format changes
        // Handle opening tags
        if (!isMarkdownStrong(prevFmt) && isMarkdownStrong(curFmt))
            m_formats.append({InlineFormat::Type::Bold, index});
        if (!prevFmt.fontItalic() && curFmt.fontItalic())
            m_formats.append({InlineFormat::Type::Italic, index});
        if (!prevFmt.fontUnderline() && curFmt.fontUnderline())
            m_formats.append({InlineFormat::Type::Underline, index});
        if (prevFmt.fontPointSize() != curFmt.fontPointSize() && curFmt.fontPointSize() != defaultFontPointSize())
            m_formats.append({InlineFormat::Type::PointSize, index, -1, {{QStringLiteral("font-size"), QString::number(int(curFmt.fontPointSize()))}}});
        if (!prevFmt.isAnchor() && curFmt.isAnchor())
            m_formats.append({InlineFormat::Type::Link, index, -1, {{QStringLiteral("href"), curFmt.anchorHref()}}});
        else if (prevFmt.isAnchor() && curFmt.isAnchor() && prevFmt.anchorHref() != curFmt.anchorHref())
            m_formats.append({InlineFormat::Type::Link, index, -1, {{QStringLiteral("href"), curFmt.anchorHref()}}});

        // Handle closing tags
        auto closeLast = [&](InlineFormat::Type type) {
            const int i = lastOfType(type);
            if (i >= 0)
                m_formats[i].end = index;
        };

        if (isMarkdownStrong(curFmt) && !isMarkdownStrong(nextFmt))
            closeLast(InlineFormat::Type::Bold);
        if (curFmt.fontItalic() && !nextFmt.fontItalic())
            closeLast(InlineFormat::Type::Italic);
        if (curFmt.fontUnderline() && !nextFmt.fontUnderline())
            closeLast(InlineFormat::Type::Underline);
        if (curFmt.fontPointSize() != nextFmt.fontPointSize() && curFmt.fontPointSize() != defaultFontPointSize())
            closeLast(InlineFormat::Type::PointSize);
        if (curFmt.isAnchor() && !nextFmt.isAnchor())
            closeLast(InlineFormat::Type::Link);
        else if (curFmt.isAnchor() && nextFmt.isAnchor() && curFmt.anchorHref() != nextFmt.anchorHref())
            closeLast(InlineFormat::Type::Link);

        previous = fragment;
        ++index;
    }
}

int InlineFormatResolver::lastOfType(InlineFormat::Type type) const
{
    for (int i = m_formats.size() - 1; i >= 0; --i) {
        if (m_formats[i].type == type && m_formats[i].end == -1)
            return i;
    }
    return -1;
}

void InlineFormatResolver::normalizeBoundaries()
{
    // Define aliases
    const int blockStart = m_block.position();
    const int blockEnd = m_block.position() + m_block.length();

    // Set the boundaries such that they won't exceed the blocks boundaries
    m_start = std::max<int>(m_start, blockStart);
    m_end = std::min<int>(m_end, blockEnd);
    if (m_start == blockStart && m_end == blockEnd)
        // Deactivate range check, because it's unnecessary
        m_useRange = false;
}
