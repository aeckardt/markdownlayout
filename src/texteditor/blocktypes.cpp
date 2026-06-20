#include "blocktypes.h"

#include "fragmentchanges.h"
#include "texteditorstyle.h"
#include "texteditorstyle_p.h"

#include <QTextBlock>
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
        // Create a QTextList object on the cursor.
        QTextList *textList = localCursor.createList(TopLevelListStyle);

        // Attach the new QTextList object to a clean (default-based) block format.
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

void setHeadingCharFormat(const QTextBlock &block, int headingLevel)
{
    auto headingFormatModifier = [&](const QTextBlock &, QTextCharFormat charFmt) {
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
    };

    QTextCursor cursor(block);
    cursor.beginEditBlock();

    // Handle existing fragments one-by-one
    applyFragmentChangesToBlock(block, headingFormatModifier);

    // Change the block char format as a fallback for empty blocks
    cursor.setBlockCharFormat(headingFormatModifier(block, defaultCharFormat()));

    cursor.endEditBlock();
}
