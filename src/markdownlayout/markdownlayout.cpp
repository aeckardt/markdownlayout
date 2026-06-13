#include "markdownlayout.h"

#include <QPainter>
#include <QTextBlock>

MarkdownLayout::MarkdownLayout(QTextDocument *doc)
    : QAbstractTextDocumentLayout(doc),
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
        if (m_blockRects.contains(block.position())) {
            rect = m_blockRects[block.position()];
        } else {
            rect = QRectF();
        }
        if (rect.isValid() && rect.intersects(clip)) {
            drawBlockDecoration(painter, block, rect);
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
        if (m_blockRects.contains(block.position())) {
            rect = m_blockRects[block.position()];
        }
        if (!rect.isValid()) {
            block = block.next();
            continue;
        }

        if (point.y() < rect.top()) {
            return block.position();
        }

        if (rect.contains(point) || (rect.top() <= point.y() && point.y() <= rect.bottom())) {
            return hitTestBlock(block, point, accuracy);
        }

        lastValidPosition = block.position() + std::max(0, block.length() - 1);
        block = block.next();
    }

    if (accuracy == Qt::HitTestAccuracy::ExactHit) {
        return -1;
    }

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

QRectF MarkdownLayout::blockBoundingRect(const QTextBlock &block) const
{
    const_cast<MarkdownLayout *>(this)->ensureLayout();

    if (!block.isValid())
        return QRectF();

    const QTextLayout *layout = block.layout();
    if (!layout)
        return QRectF();

    QRectF rect = layout->boundingRect();
    rect.moveTopLeft(layout->position());
    return rect;
}

QRectF MarkdownLayout::frameBoundingRect(QTextFrame *frame) const
{
    if (frame != document()->rootFrame()) {
        return QRectF();
    }
    return QRectF(QPointF(0.0, 0.0), m_documentSize);
}

void MarkdownLayout::documentChanged(int from, int charsRemoved, int charsAdded)
{
    Q_UNUSED(from)
    Q_UNUSED(charsRemoved)
    Q_UNUSED(charsAdded)

    QSizeF oldSize(m_documentSize);
    m_dirty = true;
    ensureLayout();

    if (m_documentSize != oldSize) {
        emit documentSizeChanged(m_documentSize);
    }
    emit update(QRectF(QPointF(0.0, 0.0), m_documentSize));
}

void MarkdownLayout::ensureLayout()
{
    if (!m_dirty) {
        return;
    }

    m_dirty = false;
    m_blockRects.clear();

    QTextDocument *doc = document();
    float docWidth = documentWidth();
    float y = m_metrics.topMargin;

    QTextOption option(doc->defaultTextOption());
    option.setWrapMode(QTextOption::WrapMode::WordWrap);

    QTextBlock block = doc->begin();
    while (block.isValid()) {
        int type = blockType(block);
        bool isListItem = block.textList();

        float textX = m_metrics.leftMargin;
        float availableWidth = docWidth - m_metrics.leftMargin - m_metrics.rightMargin;

        if (isListItem) {
            textX += m_metrics.listMarkerWidth;
            availableWidth -= m_metrics.listMarkerWidth;
        }

        if (type == BLOCK_TYPE_QUOTE || type == BLOCK_TYPE_CODE) {
            textX += m_metrics.blockPaddingX;
            availableWidth -= 2.0 * m_metrics.blockPaddingX;
        }

        availableWidth = std::max((float)80.0, availableWidth);

        QTextLayout *layout = block.layout();
        layout->clearLayout();
        layout->setFont(doc->defaultFont());
        layout->setTextOption(option);
        layout->setPosition(QPointF(textX, y + topPaddingForBlock(block)));

        float textHeight = layoutBlockText(layout, availableWidth);
        if (textHeight <= 0.0) {
            textHeight = QFontMetrics(doc->defaultFont()).height();
        }

        float blockHeight =
                topPaddingForBlock(block) +
                textHeight +
                bottomPaddingForBlock(block);

        QRectF rect(0.0, y, docWidth, blockHeight);
        m_blockRects[block.position()] = rect;

        y += blockHeight + m_metrics.paragraphSpacing;
        block = block.next();
    }

    float totalHeight = std::max(y + m_metrics.bottomMargin, m_metrics.topMargin + m_metrics.bottomMargin);
    m_documentSize = QSizeF(docWidth, totalHeight);
}

qreal MarkdownLayout::layoutBlockText(QTextLayout *layout, qreal lineWidth) const
{
    layout->beginLayout();
    float y = 0.0;
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
    }
    layout->endLayout();
    return y;
}

qreal MarkdownLayout::documentWidth() const
{
    const QTextDocument *doc = document();
    const qreal width = doc->pageSize().width(); // == doc->textWidth()
    if (width > 0.0) {
        return std::max<qreal>(80.0, width);
    }

    return m_metrics.fallbackWidth;
}

qreal MarkdownLayout::topPaddingForBlock(QTextBlock block) const
{
    int type = blockType(block);
    if (type == BLOCK_TYPE_QUOTE || type == BLOCK_TYPE_CODE) {
        return m_metrics.blockPaddingY;
    }
    return 0.0;
}

qreal MarkdownLayout::bottomPaddingForBlock(QTextBlock block) const
{
    int type = blockType(block);
    if (type == BLOCK_TYPE_QUOTE || type == BLOCK_TYPE_CODE) {
        return m_metrics.blockPaddingY;
    }
    return 0.0;
}

// # ---- Drawing helpers ----------------------------------------------

void MarkdownLayout::drawBlockDecoration(QPainter *painter, QTextBlock block, QRectF rect)
{
    int blockType = block.blockFormat().property(BLOCK_TYPE_PROPERTY).toInt();

    switch (blockType) {
    case BLOCK_TYPE_NORMAL: {
        break;
    }
    case BLOCK_TYPE_QUOTE: {
        QRectF bg = rect.adjusted(m_metrics.leftMargin / 2, 0.0, -m_metrics.rightMargin / 2, 0.0);
        painter->fillRect(bg, QColor("#f6f8fa"));

        QRectF bar = QRectF(
                    m_metrics.leftMargin / 2,
                    rect.top() + 4.0,
                    m_metrics.quoteBarWidth,
                    std::max(0.0, rect.height() - 8.0)
                );
        painter->fillRect(bar, QColor("#d0d7de"));

        break;
    }
    case BLOCK_TYPE_CODE: {
        QRectF bg = rect.adjusted(m_metrics.leftMargin / 2, 0.0, -m_metrics.rightMargin / 2, 0.0);
        painter->fillRect(bg, QColor("#f6f8fa"));

        break;
    }
    }

    if (block.textList()) {
        drawMarkdownListMarker(painter, block);
    }
}

void MarkdownLayout::drawMarkdownListMarker(QPainter *painter, QTextBlock block)
{
    const QTextLayout *layout = block.layout();
    if (layout->lineCount() == 0) {
        return;
    }

    QTextLine firstLine = layout->lineAt(0);
    qreal baselineY = layout->position().y() + firstLine.y() + firstLine.ascent();
    qreal markerX = layout->position().x() - m_metrics.listMarkerWidth + 6.0;

    painter->save();
    painter->setPen(QPen(QColor("#57606a")));
    painter->setFont(document()->defaultFont());
    painter->drawText(QPointF(markerX, baselineY), "-");
    painter->restore();
}

void MarkdownLayout::drawTextCursorIfNeeded(QPainter *painter, const PaintContext &context, QTextBlock block)
{
    int cursorPos = context.cursorPosition;
    if (cursorPos < 0)
        return;

    int blockStart = block.position();
    int blockEnd = blockStart + block.length() - 1;
    if (blockStart > cursorPos || cursorPos > blockEnd)
        return;

    QTextLayout *layout = block.layout();
    int localPos = std::max(0, std::min(cursorPos - blockStart, (int)block.text().length()));

    painter->save();
    painter->setPen(QPen(context.palette.text().color()));
    if (layout->lineCount() > 0) {
        layout->drawCursor(painter, QPointF(0.0, 0.0), localPos, 1);
    } else {
        QFontMetricsF fm = QFontMetricsF(document()->defaultFont());
        qreal x = layout->position().x();
        qreal y = layout->position().y();
        painter->drawLine(QPointF(x, y), QPointF(x, y + fm.height()));
    }
    painter->restore();
}

// # ---- Selection / hit-test helpers ---------------------------------

QList<QTextLayout::FormatRange> MarkdownLayout::selectionsForBlock(const PaintContext &context, QTextBlock block) const
{
    QList<QTextLayout::FormatRange> ranges;

    int blockStart = block.position();
    int blockEnd = blockStart + std::max(0, block.length() - 1);  // exclude paragraph separator

    for (const QAbstractTextDocumentLayout::Selection &selection : context.selections) {
        QTextCursor cursor = selection.cursor;
        if (!cursor.hasSelection()) {
            continue;
        }

        int start = std::max(cursor.selectionStart(), blockStart);
        int end = std::min(cursor.selectionEnd(), blockEnd);
        if (start >= end) {
            continue;
        }

        QTextLayout::FormatRange formatRange;
        formatRange.start = start - blockStart;
        formatRange.length = end - start;
        formatRange.format = selection.format;
        ranges.append(formatRange);
    }

    return ranges;
}

int MarkdownLayout::hitTestBlock(QTextBlock block, QPointF point, Qt::HitTestAccuracy accuracy) const
{
    QTextLayout *layout = block.layout();
    if (layout->lineCount() == 0) {
        return block.position();
    }

    int i;
    for (i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        QRectF lineRect = line.rect().translated(layout->position());
        if (point.y() <= lineRect.bottom()) {
            qreal localX = point.x() - layout->position().x();
            return block.position() + line.xToCursor(localX);
        }
    }

    if (accuracy == Qt::HitTestAccuracy::ExactHit) {
        return -1;
    }

    return block.position() + block.text().length();
}

int blockType(QTextBlock block)
{
    return block.blockFormat().property(BLOCK_TYPE_PROPERTY).toInt();
}
