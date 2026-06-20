#include "blocktypes.h"

#include "texteditorstyle.h"
#include "texteditorstyle_p.h"

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextList>

BlockType blockType(const QTextBlock &block)
{
    const QTextBlockFormat blockFmt = block.blockFormat();
    if (blockFmt.headingLevel() > 0)
        return static_cast<BlockType>((int)BlockType::Heading1 + blockFmt.headingLevel() - 1);
    if (blockFmt.objectIndex() != -1 && block.textList())
        return BlockType::TextList;
    if (blockFmt.hasProperty(QTextFormat::BlockQuoteLevel))
        return BlockType::BlockQuote;
    if (blockFmt.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
        return BlockType::HorizontalRuler;
    return BlockType::Paragraph;
}

int headingLevel(BlockType type)
{
    if (type < BlockType::Heading1 || type > BlockType::Heading4)
        return 0;
    return (int)type - (int)BlockType::Heading1 + 1;
}

BlockType headingBlockType(int headingLevel)
{
    if (headingLevel == 0)
        return BlockType::Paragraph;
    return static_cast<BlockType>((int)BlockType::Heading1 + (headingLevel - 1));
}

void setBlockTypeForSelection(const QTextCursor &cursor, BlockType type, bool toggle)
{
    QVector<QTextBlock> blocks = selectedBlocks(cursor);

    bool allOfType = true;
    for (const QTextBlock &block : blocks) {
        if (blockType(block) != type) {
            allOfType = false;
            break;
        }
    }

    BlockType newType;
    if (allOfType) {
        // If toggle is true and all affected blocks already have the requested type,
        // they are reset to Paragraph.
        if (!toggle)
            return;
        newType = BlockType::Paragraph;
    } else
        newType = type;

    QTextCursor localCursor(cursor);
    localCursor.beginEditBlock();
    for (const QTextBlock &block : blocks)
        setBlockTypeForBlock(block, newType);
    localCursor.endEditBlock();
}

void setBlockTypeForBlock(const QTextBlock &block, BlockType type, bool toggle)
{
    BlockType oldType = blockType(block);
    if (oldType == type) {
        if (toggle && oldType != BlockType::Paragraph)
            setBlockTypeForBlock(block, BlockType::Paragraph);
        return;
    }

    QTextCursor localCursor(block);
    localCursor.beginEditBlock();

    // Clear old heading format, if necessary
    int oldLevel = headingLevel(oldType);
    int level = headingLevel(type);
    if (oldLevel > 0 && level == 0)
        clearHeadingCharFormat(block);

    // Start from the default block format so switching a new block type clears old
    // block metadata such as list object index, quote level, or horizontal rule.
    QTextBlockFormat blockFmt(defaultBlockFormat());
    switch (type) {
    case BlockType::Heading1:
    case BlockType::Heading2:
    case BlockType::Heading3:
    case BlockType::Heading4: {
        setHeadingCharFormat(block, level);
        blockFmt.setHeadingLevel(level);
        break;
    }
    case BlockType::TextList: {
        // See if a QTextList object exists in the previous block
        const QTextBlock previousBlock = block.previous();
        const QTextList *textList = previousBlock.textList();

        // If not, create a new QTextList object on the cursor.
        if (!textList)
            textList = localCursor.createList(TopLevelListStyle);

        // Attach the QTextList object to a clean (default-based) block format.
        blockFmt.setObjectIndex(textList->objectIndex());
        break;
    }
    case BlockType::BlockQuote:
        blockFmt.setProperty(QTextFormat::BlockQuoteLevel, 1);
        break;
    case BlockType::HorizontalRuler:
        // Remove text in line before making horizontal ruler
        localCursor.select(QTextCursor::LineUnderCursor);
        localCursor.removeSelectedText();

        blockFmt.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, horizontalRulerWidth());
#ifdef HORIZONTAL_RULER_COLOR
        blockFmt.setProperty(QTextFormat::BackgroundBrush, horizontalRulerColor());
#endif

        break;
    default:
        // Nothing to do for paragraph
        ;
    }

    localCursor.setBlockFormat(blockFmt);
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

QTextCharFormat headingFormatModifier(int headingLevel, QTextCharFormat charFmt)
{
    // Set / remove heading-specific visual formatting
    // depending on headingLevel (0 -> no heading)
    const bool bold = isMarkdownStrong(charFmt);
    if (headingLevel > 0) {
        charFmt.setFontWeight(
                    bold
                    ? StrongFontWeight
                    : HeadingFontWeight);
        charFmt.setProperty(QTextCharFormat::Property::FontSizeAdjustment, 4 - headingLevel);
    } else {
        charFmt.setFontWeight(
                    bold
                    ? StrongFontWeight
                    : NormalFontWeight);
        charFmt.clearProperty(QTextCharFormat::Property::FontSizeAdjustment);
    }
    return charFmt;
}

void setHeadingCharFormat(const QTextBlock &block, int headingLevel)
{
    QTextCursor localCursor(block);
    localCursor.select(QTextCursor::BlockUnderCursor);
    localCursor.beginEditBlock();

    // Handle existing fragments one-by-one
    modifyCharFormatPerFragment(localCursor, [&](const QTextBlock &, QTextCharFormat charFmt) {
        return headingFormatModifier(headingLevel, charFmt);
    });

    // Change the block char format as a fallback for empty blocks
    localCursor.setBlockCharFormat(headingFormatModifier(headingLevel, defaultCharFormat()));

    localCursor.endEditBlock();
}

struct CharFormatUpdate {
    int start;
    int end;
    QTextCharFormat newCharFmt;
};

void modifyCharFormatPerFragment(const QTextCursor &cursor, const CharFormatModifier &modifier)
{
    if (!cursor.hasSelection())
        return;

    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    QTextDocument *doc = cursor.document();
    QTextBlock block = doc->findBlock(cursor.selectionStart());

    QVector<CharFormatUpdate> updates;

    while (block.position() <= endPos && block.isValid()) {
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
        block = block.next();
    }

    QTextCursor localCursor(doc);
    localCursor.beginEditBlock();
    for (const CharFormatUpdate &update : updates) {
        localCursor.setPosition(update.start);
        localCursor.setPosition(update.end, QTextCursor::MoveMode::KeepAnchor);
        localCursor.setCharFormat(update.newCharFmt);
    }
    localCursor.endEditBlock();
}
