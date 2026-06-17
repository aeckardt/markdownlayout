#ifndef INLINEFORMATRESOLVER_H
#define INLINEFORMATRESOLVER_H

#include <QMap>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFragment>
#include <QVector>

struct InlineFormat {
    enum Type {
        Bold = 0,
        Italic,
        Underline,
        PointSize,
        Link
    };

    Type type;
    int start;
    int end;
    QMap<QString, QString> attrs;

    InlineFormat(Type type, int start, int end = -1)
        : type(type), start(start), end(end), attrs(QMap<QString, QString>()) {}
    InlineFormat(Type type, int start, int end, QMap<QString, QString> attrs)
        : type(type), start(start), end(end), attrs(attrs) {}
};

struct FormatChange {
    InlineFormat::Type type;
    QMap<QString, QString> attrs;
    bool open;
};

struct ExportableFragment {
    QTextFragment fragment;
    QVector<FormatChange> formatChanges;
};

/*
 * InlineFormatResolver is a helper class to export a QTextBlock into HTML and Markdown.
 * It organizes the nested char formats in the fragments such that the inner/outer tags
 * don't conflict. For instance, we want to avoid output like this:
 *
 *     <strong><em>This is an </strong>example</em>
 *
 * The problem here is that the inner/outer tags do not match. A lookahead is required
 * to get the output right.
 * Therefore, InlineFormatResolver organizes the format differences between fragments such
 * that it's clear which format has to be the outer and which one the inner.
 * If both cases are equally possible, there is a precedence list which one takes priority.
 */
class InlineFormatResolver {
public:
    InlineFormatResolver(const QTextBlock &block, int start = -1, int end = -1);
    QVector<ExportableFragment> fragments() const { return m_fragments; }

private:
    void normalizeBoundaries();
    void detectFormatChanges();
    void cleanUp();
    void resolveChangeStack();
    int lastOfType(InlineFormat::Type type) const;

    QTextBlock m_block;
    int m_start;
    int m_end;
    bool m_useRange;
    int m_firstIndex;
    int m_lastIndex;
    QVector<InlineFormat> m_formats;
    QVector<ExportableFragment> m_fragments;
};

#endif
