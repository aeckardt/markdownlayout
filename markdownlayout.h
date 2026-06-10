#ifndef MARKDOWNLAYOUT_H
#define MARKDOWNLAYOUT_H

#include <QAbstractTextDocumentLayout>
#include <QMap>

class LayoutMetrics
{
public:
    float fallbackWidth = 760.0;
    float leftMargin = 28.0;
    float rightMargin = 28.0;
    float topMargin = 24.0;
    float bottomMargin = 24.0;
    float paragraphSpacing = 8.0;
    float listMarkerWidth = 24.0;
    float blockPaddingX = 12.0;
    float blockPaddingY = 8.0;
    float quoteBarWidth = 4.0;
};

class MarkdownLayout : public QAbstractTextDocumentLayout
{
    Q_OBJECT

public:
    MarkdownLayout(QTextDocument *doc);

    void draw(QPainter *painter, const PaintContext &context) override;
    int hitTest(const QPointF &point, Qt::HitTestAccuracy accuracy) const override;
    int pageCount() const override;
    QSizeF documentSize() const override;
    QRectF blockBoundingRect(const QTextBlock &block) const override;
    QRectF frameBoundingRect(QTextFrame *frame) const override;

protected:
    void documentChanged(int from, int charsRemoved, int charsAdded) override;

private:
    LayoutMetrics m_metrics;
    bool m_dirty;
    QSizeF m_documentSize;
    QMap<int, QRectF> m_blockRects;

    void ensureLayout();
    float layoutBlockText(QTextLayout *layout, float lineWidth) const;
    float documentWidth() const;
    float topPaddingForBlock(QTextBlock block) const;
    float bottomPaddingForBlock(QTextBlock block) const;

    void drawBlockDecoration(QPainter *painter, QTextBlock block, QRectF rect);
    void drawMarkdownListMarker(QPainter *painter, QTextBlock block);
    void drawTextCursorIfNeeded(QPainter *painter, const PaintContext &context, QTextBlock block);

    QList<QTextLayout::FormatRange> selectionsForBlock(const PaintContext &context, QTextBlock block) const;
    int hitTestBlock(QTextBlock block, QPointF point, Qt::HitTestAccuracy accuracy) const;
};

int blockType(QTextBlock block);

#endif // MARKDOWNLAYOUT_H
