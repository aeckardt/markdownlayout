#include "fragmentchanges.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

void applyFragmentChangesToRange(const QVector<QTextBlock> &blocks,
                                 int startPos, int endPos,
                                 const CharFormatModifier &modifier);

void applyFragmentChangesToSelection(const QTextCursor &cursor, const CharFormatModifier &modifier)
{
    applyFragmentChangesToRange(
                selectedBlocks(cursor),
                cursor.selectionStart(),
                cursor.selectionEnd(),
                modifier);
}

void applyFragmentChangesToBlock(const QTextBlock &block, const CharFormatModifier &modifier)
{
    applyFragmentChangesToRange(
                {block},
                block.position(),
                block.position() + block.length(),
                modifier);
}

struct CharFormatUpdate {
    int start;
    int end;
    QTextCharFormat newCharFmt;
};

void applyFragmentChangesToRange(const QVector<QTextBlock> &blocks,
                                 int startPos, int endPos,
                                 const CharFormatModifier &modifier)
{
    if (blocks.isEmpty())
        return;

    QVector<CharFormatUpdate> updates;

    for (const QTextBlock &block : blocks) {
        QTextBlock::iterator it = block.begin();
        while (!it.atEnd()) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid()) {
                int fragmentStart = fragment.position();
                int fragmentEnd = fragment.position() + fragment.length();

                int start = std::max<int>(startPos, fragmentStart);
                int end = std::min<int>(endPos, fragmentEnd);

                if (start < end) {
                    const QTextCharFormat charFmt = fragment.charFormat();
                    updates.append({start, end, modifier(block, charFmt)});
                }
            }
            ++it;
        }
    }

    // Not pretty, but better than an extra parameter
    QTextDocument *doc = const_cast<QTextDocument *>(blocks.at(0).document());

    QTextCursor localCursor(doc);
    localCursor.beginEditBlock();
    for (const CharFormatUpdate &update : updates) {
        localCursor.setPosition(update.start);
        localCursor.setPosition(update.end, QTextCursor::MoveMode::KeepAnchor);
        localCursor.setCharFormat(update.newCharFmt);
    }
    localCursor.endEditBlock();
}

QVector<QTextBlock> selectedBlocks(const QTextCursor &cursor)
{
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    if (start == end) {
        if (!cursor.block().isValid())
            return {};
        return {cursor.block()};
    }

    QTextDocument *doc = cursor.document();

    QTextBlock firstBlock = doc->findBlock(start);
    QTextBlock lastBlock = doc->findBlock(end);

    //  If the selection ends exactly at the beginning of a block, that block
    //  is not part of the user's visible selection.
    if (end == lastBlock.position() && lastBlock != firstBlock)
        lastBlock = lastBlock.previous();

    QVector<QTextBlock> blocks;
    QTextBlock block = firstBlock;
    while (block.isValid()) {
        blocks.append(block);
        if (block == lastBlock)
            break;
        block = block.next();
    }

    return blocks;
}
