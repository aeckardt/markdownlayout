#ifndef MARKDOWNLAYOUT_H
#define MARKDOWNLAYOUT_H

#include <QAbstractTextDocumentLayout>
#include <QMap>

class LayoutMetrics
{
public:
    qreal fallbackWidth = 760.0;
    qreal paragraphSpacing = 4.0;
    qreal listBulletLeftMargin = 7.0;
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
    void ensureLayout();
    qreal layoutBlockText(QTextLayout *layout, qreal lineX, qreal lineWidth) const;
    qreal documentWidth() const;
    qreal topPaddingForBlock(const QTextBlock &block) const;
    qreal bottomPaddingForBlock(const QTextBlock &block) const;
    qreal blockIndent(const QTextBlock &block) const;

    void drawBlock(QPainter *painter, const PaintContext &context, const QTextBlock &block, QRectF rect);
    void drawListItem(QPainter *painter, const PaintContext &context, const QTextBlock &block);
    void drawTextCursorIfNeeded(QPainter *painter, const PaintContext &context, const QTextBlock &block);

    QList<QTextLayout::FormatRange> selectionsForBlock(const PaintContext &context, const QTextBlock &block) const;
    int hitTestBlock(const QTextBlock &block, QPointF point, Qt::HitTestAccuracy accuracy) const;

    LayoutMetrics m_metrics;
    bool m_dirty;
    QSizeF m_documentSize;
    QMap<int, QRectF> m_blockRects;
};

#endif // MARKDOWNLAYOUT_H
