// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QTEXTDOCUMENTLAYOUT_H
#define QTEXTDOCUMENTLAYOUT_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "QtGui/qabstracttextdocumentlayout.h"
#include "QtGui/qtextoption.h"
#include "QtGui/qtextobject.h"
#include "qcssparser_p.h"
#include "qfixed_p.h"

QT_BEGIN_NAMESPACE

class QTextListFormat;
class QTextTableCell;

class QTextTableData;
class QTextFrameData;
struct QTextLayoutStruct;
class QBasicTimer;
struct QCheckPoint;
struct EdgeData;

/*
 * The following class has been copied and merged from Qt 6.4.1 sources
 * mainly from QTextDocumentLayout and QTextDocumentLayoutPrivate
 * in order to test and modify text document layout behavior
 * close to the default / original
 */
class Q_GUI_EXPORT QTextDocumentLayout : public QAbstractTextDocumentLayout
{
    Q_OBJECT
    Q_PROPERTY(int cursorWidth READ cursorWidth WRITE setCursorWidth)
    Q_PROPERTY(qreal idealWidth READ idealWidth)
    Q_PROPERTY(bool contentHasAlignment READ contentHasAlignment)
public:
    // <--- from QTextDocumentLayout --->
    explicit QTextDocumentLayout(QTextDocument *doc);
    ~QTextDocumentLayout();

    // from the abstract layout
    void draw(QPainter *painter, const PaintContext &context) override;
    int hitTest(const QPointF &point, Qt::HitTestAccuracy accuracy) const override;

    int pageCount() const override;
    QSizeF documentSize() const override;

    void setCursorWidth(int width);
    int cursorWidth() const;

    // internal, to support the ugly FixedColumnWidth wordwrap mode in QTextEdit
    void setFixedColumnWidth(int width);

    // internal for QTextEdit's NoWrap mode
    void setViewport(const QRectF &viewport);

    virtual QRectF frameBoundingRect(QTextFrame *frame) const override;
    virtual QRectF blockBoundingRect(const QTextBlock &block) const override;
    QRectF tableBoundingRect(QTextTable *table) const;
    QRectF tableCellBoundingRect(QTextTable *table, const QTextTableCell &cell) const;

    // ####
    int layoutStatus() const;
    int dynamicPageCount() const;
    QSizeF dynamicDocumentSize() const;
    void ensureLayouted(qreal);

    qreal idealWidth() const;

    bool contentHasAlignment() const;

protected:
    void documentChanged(int from, int oldLength, int length) override;
    void resizeInlineObject(QTextInlineObject item, int posInDocument, const QTextFormat &format) override;
    void positionInlineObject(QTextInlineObject item, int posInDocument, const QTextFormat &format) override;
    void drawInlineObject(QPainter *p, const QRectF &rect, QTextInlineObject item,
                          int posInDocument, const QTextFormat &format) override;
    virtual void timerEvent(QTimerEvent *e) override;
private:
    QRectF doLayout(int from, int oldLength, int length);
    void layoutFinished();

    QTextOption::WrapMode wordWrapMode;
#ifdef LAYOUT_DEBUG
    mutable QString debug_indent;
#endif

    int fixedColumnWidth;
    int m_cursorWidth;

    QSizeF lastReportedSize;
    QRectF viewportRect;
    QRectF clipRect;

    mutable int currentLazyLayoutPosition;
    mutable int lazyLayoutStepSize;
    QBasicTimer *layoutTimer;
    mutable QBasicTimer *sizeChangedTimer;
    uint showLayoutProgress : 1;
    uint insideDocumentChange : 1;

    int lastPageCount;
    qreal m_idealWidth;
    bool m_contentHasAlignment;

    QFixed blockIndent(const QTextBlockFormat &blockFormat) const;

    void drawFrame(const QPointF &offset, QPainter *painter, const QAbstractTextDocumentLayout::PaintContext &context,
                   QTextFrame *f) const;
    void drawFlow(const QPointF &offset, QPainter *painter, const QAbstractTextDocumentLayout::PaintContext &context,
                  QTextFrame::Iterator it, const QList<QTextFrame *> &floats, QTextBlock *cursorBlockNeedingRepaint) const;
    void drawBlock(const QPointF &offset, QPainter *painter, const QAbstractTextDocumentLayout::PaintContext &context,
                   const QTextBlock &bl, bool inRootFrame) const;
    void drawListItem(const QPointF &offset, QPainter *painter, const QAbstractTextDocumentLayout::PaintContext &context,
                      const QTextBlock &bl, const QTextCharFormat *selectionFormat) const;
    void drawTableCellBorder(const QRectF &cellRect, QPainter *painter, QTextTable *table, QTextTableData *td, const QTextTableCell &cell) const;
    void drawTableCell(const QRectF &cellRect, QPainter *painter, const QAbstractTextDocumentLayout::PaintContext &cell_context,
                       QTextTable *table, QTextTableData *td, int r, int c,
                       QTextBlock *cursorBlockNeedingRepaint, QPointF *cursorBlockOffset) const;
    void drawBorder(QPainter *painter, const QRectF &rect, qreal topMargin, qreal bottomMargin, qreal border,
                    const QBrush &brush, QTextFrameFormat::BorderStyle style) const;
    void drawFrameDecoration(QPainter *painter, QTextFrame *frame, QTextFrameData *fd, const QRectF &clip, const QRectF &rect) const;

public:
    // <--- from QTextDocumentLayoutPrivate --->
    qreal scaleToDevice(qreal value) const;
    QFixed scaleToDevice(QFixed value) const;

private:
    enum HitPoint {
        PointBefore,
        PointAfter,
        PointInside,
        PointExact
    };
    HitPoint hitTest(QTextFrame *frame, const QFixedPoint &point, int *position, QTextLayout **l, Qt::HitTestAccuracy accuracy) const;
    HitPoint hitTest(QTextFrame::Iterator it, HitPoint hit, const QFixedPoint &p,
                     int *position, QTextLayout **l, Qt::HitTestAccuracy accuracy) const;
    HitPoint hitTest(QTextTable *table, const QFixedPoint &point, int *position, QTextLayout **l, Qt::HitTestAccuracy accuracy) const;
    HitPoint hitTest(const QTextBlock &bl, const QFixedPoint &point, int *position, QTextLayout **l, Qt::HitTestAccuracy accuracy) const;

    QTextLayoutStruct layoutCell(QTextTable *t, const QTextTableCell &cell, QFixed width,
                                 int layoutFrom, int layoutTo, QTextTableData *tableData, QFixed absoluteTableY,
                                 bool withPageBreaks);
    void setCellPosition(QTextTable *t, const QTextTableCell &cell, const QPointF &pos);
    QRectF layoutTable(QTextTable *t, int layoutFrom, int layoutTo, QFixed parentY);

    void positionFloat(QTextFrame *frame, QTextLine *currentLine = nullptr);

    // calls the next one
    QRectF layoutFrame(QTextFrame *f, int layoutFrom, int layoutTo, QFixed parentY = 0);
    QRectF layoutFrame(QTextFrame *f, int layoutFrom, int layoutTo, QFixed frameWidth, QFixed frameHeight, QFixed parentY = 0);

    void layoutBlock(const QTextBlock &bl, int blockPosition, const QTextBlockFormat &blockFormat,
                     QTextLayoutStruct *layoutStruct, int layoutFrom, int layoutTo, const QTextBlockFormat *previousBlockFormat);
    void layoutFlow(QTextFrame::Iterator it, QTextLayoutStruct *layoutStruct, int layoutFrom, int layoutTo, QFixed width = 0);

    void floatMargins(QFixed y, const QTextLayoutStruct *layoutStruct, QFixed *left, QFixed *right) const;
    QFixed findY(QFixed yFrom, const QTextLayoutStruct *layoutStruct, QFixed requiredWidth) const;

    QList<QCheckPoint> checkPoints;

    QTextFrame::Iterator frameIteratorForYPosition(QFixed y) const;
    QTextFrame::Iterator frameIteratorForTextPosition(int position) const;

    void ensureLayouted(QFixed y) const;
    void ensureLayoutedByPosition(int position) const;
    inline void ensureLayoutFinished() const
    { ensureLayoutedByPosition(INT_MAX); }
    void layoutStep() const;

    QRectF frameBoundingRectInternal(QTextFrame *frame) const;

    inline bool canLayout() const { return document()->isLayoutEnabled() && !document()->pageSize().isNull(); }
};

QT_END_NAMESPACE

#endif // QTEXTDOCUMENTLAYOUT_P_H
