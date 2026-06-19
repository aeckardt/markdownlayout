#include "markdownlayout.h"

#include <QPainter>
#include <QTextBlock>
#include <QTextList>
#include <QTextListFormat>

MarkdownLayout::MarkdownLayout(QTextDocument *doc)
    : QAbstractTextDocumentLayout(doc),
      m_dirty(false),
      m_documentSize(m_metrics.fallbackWidth, 0.0)
{
}

void MarkdownLayout::draw(QPainter *painter, const PaintContext &context)
{
    ensureLayout();

    QRectF clip = context.clip;
    if (!clip.isValid())
        clip = QRectF(QPointF(0.0, 0.0), documentSize());

    painter->save();
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        QRectF rect;
        if (m_blockRects.contains(block.position()))
            rect = m_blockRects[block.position()];
        if (rect.isValid() && rect.intersects(clip)) {
            drawBlock(painter, context, block, rect);
            auto selections = selectionsForBlock(context, block);
            block.layout()->draw(painter, QPointF(0.0, 0.0), selections, clip);
            drawTextCursorIfNeeded(painter, context, block);
        }
        block = block.next();
    }
    painter->restore();
}

int MarkdownLayout::hitTest(const QPointF &point, Qt::HitTestAccuracy accuracy) const
{
    const_cast<MarkdownLayout *>(this)->ensureLayout();

    QTextBlock block = document()->begin();
    int lastValidPosition = 0;

    while (block.isValid()) {
        QRectF rect;
        if (m_blockRects.contains(block.position()))
            rect = m_blockRects[block.position()];
        if (!rect.isValid()) {
            block = block.next();
            continue;
        }

        if (point.y() < rect.top())
            return block.position();

        if (rect.contains(point) || (rect.top() <= point.y() && point.y() <= rect.bottom()))
            return hitTestBlock(block, point, accuracy);

        lastValidPosition = block.position() + std::max(0, block.length() - 1);
        block = block.next();
    }

    if (accuracy == Qt::HitTestAccuracy::ExactHit)
        return -1;

    return std::min(lastValidPosition, std::max(0, document()->characterCount() - 1));
}

int MarkdownLayout::pageCount() const
{
    return 1;
}

QSizeF MarkdownLayout::documentSize() const
{
    const_cast<MarkdownLayout *>(this)->ensureLayout();
    return m_documentSize;
}

/*
 * blockBoundingRect is not the decorated paint rect.
 * It is the public QTextBlock geometry used by QTextEdit/QWidgetTextControl.
 *
 * It must include all QTextLines, but it should also include the layout
 * origin because QTextLines may be shifted to the right for Markdown
 * decorations such as list markers or blockquote padding.
 */
QRectF MarkdownLayout::blockBoundingRect(const QTextBlock &block) const
{
    const_cast<MarkdownLayout *>(this)->ensureLayout();

    if (!block.isValid())
        return QRectF();

    const QTextLayout *layout = block.layout();
    if (!layout)
        return QRectF();

    QRectF rect = layout->boundingRect();

    // QTextLayout::boundingRect() may start at x > 0 if lines are shifted
    // via QTextLine::setPosition(lineX, ...). QWidgetTextControl expects
    // blockBoundingRect().topLeft() to behave like the QTextLayout origin,
    // but the rect should still contain all lines.
    if (rect.left() > 0.0)
        rect.setLeft(0.0);

    if (rect.top() > 0.0)
        rect.setTop(0.0);

    rect.translate(layout->position());
    return rect;
}

QRectF MarkdownLayout::frameBoundingRect(QTextFrame *frame) const
{
    if (frame != document()->rootFrame())
        return QRectF();
    return QRectF(QPointF(0.0, 0.0), m_documentSize);
}

void MarkdownLayout::documentChanged(int, int, int)
{
    const QSizeF oldSize = m_documentSize;

    m_dirty = true;
    ensureLayout();

    if (m_documentSize != oldSize)
        emit documentSizeChanged(m_documentSize);

    // Make sure deleted content is cleared
    const QRectF oldRect(QPointF(0.0, 0.0), oldSize);
    const QRectF newRect(QPointF(0.0, 0.0), m_documentSize);

    emit update(oldRect.united(newRect));
}

void MarkdownLayout::ensureLayout()
{
    if (!m_dirty)
        return;

    m_dirty = false;
    m_blockRects.clear();

    QTextDocument *doc = document();
    float docWidth = documentWidth();
    float y = 0.0;

    QTextOption option(doc->defaultTextOption());
    option.setWrapMode(QTextOption::WrapMode::WordWrap);

    QTextBlock block = doc->begin();
    while (block.isValid()) {
        QTextBlockFormat blockFmt = block.blockFormat();
        bool isBlockQuote = blockFmt.hasProperty(QTextFormat::BlockQuoteLevel);

        // QTextLine positions carry the visual indentation.
        // The list marker is painted later into the indentation area by drawListItem().
        qreal textX = blockIndent(block);
        qreal availableWidth = docWidth - textX;

        if (isBlockQuote) {
            textX += m_metrics.blockPaddingX;
            availableWidth -= 2.0 * m_metrics.blockPaddingX;
        }

        availableWidth = std::max((qreal)80.0, availableWidth);

        QTextLayout *layout = block.layout();
        layout->clearLayout();
        layout->setFont(doc->defaultFont());
        layout->setTextOption(option);
        layout->setPosition(QPointF(0.0, y + topPaddingForBlock(block)));

        qreal textHeight = layoutBlockText(layout, textX, availableWidth);
        if (textHeight <= 0.0)
            textHeight = QFontMetrics(doc->defaultFont()).height();

        qreal blockHeight =
                topPaddingForBlock(block) +
                textHeight +
                bottomPaddingForBlock(block);

        QRectF rect(0.0, y, docWidth, blockHeight);
        m_blockRects[block.position()] = rect;

        y += blockHeight + m_metrics.paragraphSpacing;
        block = block.next();
    }

    float totalHeight = std::max<float>(y, 0.0);
    m_documentSize = QSizeF(docWidth, totalHeight);
}

qreal MarkdownLayout::layoutBlockText(QTextLayout *layout, qreal lineX, qreal lineWidth) const
{
    layout->beginLayout();
    qreal y = 0.0;
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid())
            break;

        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(lineX, y));

        y += line.height();
    }
    layout->endLayout();
    return y;
}

qreal MarkdownLayout::documentWidth() const
{
    const QTextDocument *doc = document();
    const qreal width = doc->pageSize().width(); // == doc->textWidth()
    if (width > 0.0)
        return std::max<qreal>(80.0, width);

    return m_metrics.fallbackWidth;
}

qreal MarkdownLayout::topPaddingForBlock(const QTextBlock &block) const
{
    QTextBlockFormat blockFmt = block.blockFormat();
    if (blockFmt.hasProperty(QTextFormat::BlockQuoteLevel)
            || blockFmt.hasProperty(QTextFormat::BlockCodeFence))
        return m_metrics.blockPaddingY;
    return 0.0;
}

qreal MarkdownLayout::bottomPaddingForBlock(const QTextBlock &block) const
{
    QTextBlockFormat blockFmt = block.blockFormat();
    if (blockFmt.hasProperty(QTextFormat::BlockQuoteLevel)
            || blockFmt.hasProperty(QTextFormat::BlockCodeFence))
        return m_metrics.blockPaddingY;
    return 0.0;
}

/*
 * Match Qt's internal indent model: the visual x-offset is the
 * sum of the block indent and the QTextListFormat indent, multiplied by
 * document()->indentWidth().
 *
 * List nesting is stored per block via QTextBlockFormat::indent().
 * QTextListFormat::indent() remains the list-wide base indent; normally it is 1
 * for top-level lists.
 */
qreal MarkdownLayout::blockIndent(const QTextBlock &block) const
{
    int indent = block.blockFormat().indent();
    QTextList *textList = block.textList();
    if (textList)
        indent += textList->format().indent();

    if (indent == 0)
        return 0.0;

    return qreal(indent) * document()->indentWidth();
}

// ---- Drawing helpers ----------------------------------------------

void MarkdownLayout::drawBlock(QPainter *painter, const PaintContext &context, const QTextBlock &block, QRectF rect)
{
    QTextBlockFormat blockFmt = block.blockFormat();
    bool isListItem = block.textList();
    bool isBlockQuote = blockFmt.hasProperty(QTextFormat::BlockQuoteLevel);
    bool isCodeBlock = blockFmt.hasProperty(QTextFormat::BlockCodeFence);

    if (isListItem)
        drawListItem(painter, context, block);
    else if (isBlockQuote) {
        painter->fillRect(rect, QColor("#f6f8fa"));
        QRectF bar = QRectF(
                    0.0,
                    rect.top() + 4.0,
                    m_metrics.quoteBarWidth,
                    std::max(0.0, rect.height() - 8.0)
                );
        painter->fillRect(bar, QColor("#d0d7de"));
    } else if (isCodeBlock)
        painter->fillRect(rect, QColor("#f6f8fa"));
}

void MarkdownLayout::drawListItem(QPainter *painter, const PaintContext &context, const QTextBlock &block)
{
    const QTextBlockFormat blockFmt = block.blockFormat();
    const QTextCharFormat charFmt = block.charFormat();
    QFont font(charFmt.font());
    const QFontMetrics fontMetrics(font);

    // A list should be available in this method
    QTextList *textList = block.textList();
    Q_ASSERT(textList);

    // The list object provides the default QTextListFormat::style(), but Qt also
    // supports a per-item override via QTextBlockFormat::ListStyle.
    QTextListFormat listFmt = textList->format();
    int style = listFmt.style();
    if (blockFmt.hasProperty(QTextFormat::ListStyle))
        style = QTextListFormat::Style(blockFmt.intProperty(QTextFormat::ListStyle));

    QTextLayout *layout = block.layout();
    if (layout->lineCount() == 0)
        return;

    QTextLine firstLine = layout->lineAt(0);
    Q_ASSERT(firstLine.isValid());

    QPointF pos = layout->position();
    QRectF textRect = firstLine.naturalTextRect();
    pos += textRect.topLeft().toPoint();

    QSizeF size;
    switch (style) {
    case QTextListFormat::ListDisc:
    case QTextListFormat::ListCircle:
    case QTextListFormat::ListSquare:
        size.setWidth(fontMetrics.lineSpacing() / 3);
        size.setHeight(size.width());
        break;

    default:
        return;
    }

    // The text line starts at the indented text position. Move back by one document
    // indent width to paint the marker inside the indentation area.
    QRectF rct(pos, size);
    rct.translate(-document()->indentWidth() + m_metrics.listBulletLeftMargin,
                  (fontMetrics.height() / 2) - (size.height() / 2));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QBrush brush = context.palette.brush(QPalette::Text);

    switch (style) {
    case QTextListFormat::ListSquare:
        painter->fillRect(rct, brush);
        break;
    case QTextListFormat::ListCircle:
        painter->setPen(QPen(brush, 0));
        painter->drawEllipse(rct.translated(0.5, 0.5));  // pixel align for sharper rendering
        break;
    case QTextListFormat::ListDisc:
        painter->setBrush(brush);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(rct);
        break;
    case QTextListFormat::ListStyleUndefined:
        break;
    default:
        break;
    }

    painter->restore();
}

void MarkdownLayout::drawTextCursorIfNeeded(QPainter *painter, const PaintContext &context, const QTextBlock &block)
{
    int cursorPos = context.cursorPosition;
    if (cursorPos < 0)
        return;

    int blockStart = block.position();
    int blockEnd = blockStart + block.length();
    if (cursorPos < blockStart || cursorPos >= blockEnd)
        return;

    QTextLayout *layout = block.layout();
    int localPos = cursorPos - blockStart;  // cursor position within block

    painter->save();
    painter->setPen(QPen(context.palette.text().color()));
    if (layout->lineCount() > 0)
        layout->drawCursor(painter, QPointF(), localPos);
    else {
        QFontMetricsF fm = QFontMetricsF(document()->defaultFont());
        qreal x = layout->position().x();
        qreal y = layout->position().y();
        painter->drawLine(QPointF(x, y), QPointF(x, y + fm.height()));
    }
    painter->restore();
}

// ---- Selection / hit-test helpers ---------------------------------

QList<QTextLayout::FormatRange> MarkdownLayout::selectionsForBlock(const PaintContext &context, const QTextBlock &block) const
{
    QList<QTextLayout::FormatRange> ranges;

    int blockStart = block.position();
    int blockEnd = blockStart + std::max(0, block.length() - 1);  // exclude paragraph separator

    for (const QAbstractTextDocumentLayout::Selection &selection : context.selections) {
        QTextCursor cursor = selection.cursor;
        if (!cursor.hasSelection())
            continue;

        int start = std::max(cursor.selectionStart(), blockStart);
        int end = std::min(cursor.selectionEnd(), blockEnd);
        if (start >= end)
            continue;

        QTextLayout::FormatRange formatRange;
        formatRange.start = start - blockStart;
        formatRange.length = end - start;
        formatRange.format = selection.format;
        ranges.append(formatRange);
    }

    return ranges;
}

int MarkdownLayout::hitTestBlock(const QTextBlock &block, QPointF point, Qt::HitTestAccuracy accuracy) const
{
    QTextLayout *layout = block.layout();
    if (layout->lineCount() == 0)
        return block.position();

    int i;
    for (i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        QRectF lineRect = line.rect().translated(layout->position());
        if (point.y() <= lineRect.bottom()) {
            qreal localX = point.x() - layout->position().x();
            return block.position() + line.xToCursor(localX);
        }
    }

    if (accuracy == Qt::ExactHit)
        return -1;

    return block.position() + block.text().length();
}
