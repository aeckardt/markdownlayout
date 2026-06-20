#ifndef BLOCKTYPES_H
#define BLOCKTYPES_H

class QTextBlock;
class QTextCursor;

// Markdown-like block types. Each of them corresponds mainly to certain QTextBlockFormat
// properties. Headings also imply char format modifications. They are treated as exclusive
// types, not as freely composable QTextBlockFormat features.
enum class BlockType : int {
    Paragraph       = 0,
    Heading1        = 10,
    Heading2        = 11,
    Heading3        = 12,
    Heading4        = 13,
    TextList        = 20,
    BlockQuote      = 30,
    HorizontalRuler = 40
};

BlockType blockType(const QTextBlock &block);

int headingLevel(BlockType type);
inline bool isHeading(BlockType type) { return headingLevel(type) > 0; }
BlockType headingBlockType(int headingLevel);

void setBlockTypeForSelection(const QTextCursor &cursor, BlockType type, bool toggle = false);
void setBlockTypeForBlock(const QTextBlock &block, BlockType type, bool toggle = false);

void setHeadingCharFormat(const QTextBlock &block, int headingLevel);
inline void clearHeadingCharFormat(const QTextBlock &block)
{ setHeadingCharFormat(block, 0); }

#endif // BLOCKTYPES_H
