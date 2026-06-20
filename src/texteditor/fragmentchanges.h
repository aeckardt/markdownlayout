#ifndef FRAGMENTCHANGES_H
#define FRAGMENTCHANGES_H

#include <QVector>
#include <functional>

class QTextBlock;
class QTextCharFormat;
class QTextCursor;

typedef std::function<QTextCharFormat(const QTextBlock &, QTextCharFormat)> CharFormatModifier;

void applyFragmentChangesToSelection(const QTextCursor &cursor, const CharFormatModifier &modifier);
void applyFragmentChangesToBlock(const QTextBlock &block, const CharFormatModifier &modifier);

QVector<QTextBlock> selectedBlocks(const QTextCursor &cursor);

#endif // FRAGMENTCHANGES_H
