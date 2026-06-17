#ifndef MARKDOWNLAYOUT_H
#define MARKDOWNLAYOUT_H

#include <QAbstractTextDocumentLayout>
#include <QMap>

class LayoutMetrics
{
public:
    qreal fallbackWidth = 760.0;
    float paragraphSpacing = 4.0;
    qreal listMarkerWidth = 24.0;
    qreal blockPaddingX = 12.0;
    qreal blockPaddingY = 8.0;
    qreal quoteBarWidth = 4.0;
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
    void documentChanged(int, int, int) override;

private:
    LayoutMetrics m_metrics;
    bool m_dirty;
    QSizeF m_documentSize;
    QMap<int, QRectF> m_blockRects;

    void ensureLayout();
    qreal layoutBlockText(QTextLayout *layout, qreal lineX, qreal lineWidth) const;
    qreal documentWidth() const;
    qreal topPaddingForBlock(QTextBlock block) const;
    qreal bottomPaddingForBlock(QTextBlock block) const;

    void drawBlockDecoration(QPainter *painter, QTextBlock block, QRectF rect);
    void drawMarkdownListMarker(QPainter *painter, QTextBlock block);
    void drawTextCursorIfNeeded(QPainter *painter, const PaintContext &context, QTextBlock block);

    QList<QTextLayout::FormatRange> selectionsForBlock(const PaintContext &context, QTextBlock block) const;
    int hitTestBlock(QTextBlock block, QPointF point, Qt::HitTestAccuracy accuracy) const;
};

#endif // MARKDOWNLAYOUT_H
