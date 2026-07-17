#include "htmlstyle.h"

#include <QByteArray>
#include <QFont>
#include <QHash>
#include <QList>
#include <QTextFormat>

struct CssFloat {
    double value;
    QByteArray unit;
};

static bool parseCssFloat(const QByteArray &raw, CssFloat &f);

bool applyCssToCharFormat(const CssDeclarations &style, QTextCharFormat &charFmt)
{
    bool changed = false;

    // Apply parsed CSS declarations to QTextCharFormat
    if (style.contains("font-size")) {
        CssFloat fontSize;
        const QByteArray raw = style.value("font-size").toLower();
        if (parseCssFloat(raw, fontSize)) {
            if (fontSize.unit == "pt") {
                charFmt.setFontPointSize(fontSize.value);
                changed = true;
            } else if (fontSize.unit == "px") {
                charFmt.setProperty(QTextFormat::FontPixelSize, int(fontSize.value));
                changed = true;
            } else
                qWarning() << "Unsupported font-size unit ignored:" << raw;
        } else {
            static const QHash<QByteArray, int> namedSizes {
                {"xx-small", 8},
                {"x-small",  9},
                {"small",    10},
                {"medium",   12},
                {"large",    14},
                {"x-large",  16},
                {"xx-large", 18},
            };
            const auto size = namedSizes.value(raw, -1);
            if (size > 0) {
                charFmt.setFontPointSize(size);
                changed = true;
            } else
                qWarning() << "Unsupported font-size ignored:" << raw;
        }
    }

    if (style.contains("font-weight")) {
        const QByteArray raw = style.value("font-weight").toLower();
        static const QHash<QByteArray, QFont::Weight> weights {
            {"normal",  QFont::Normal},
            {"bold",    QFont::Bold},
            {"bolder",  QFont::Bold},
            {"lighter", QFont::Light},
            {"100",     QFont::Thin},
            {"200",     QFont::ExtraLight},
            {"300",     QFont::Light},
            {"400",     QFont::Normal},
            {"500",     QFont::Medium},
            {"600",     QFont::DemiBold},
            {"700",     QFont::Bold},
            {"800",     QFont::ExtraBold},
            {"900",     QFont::Black},
        };
        if (weights.contains(raw)) {
            charFmt.setFontWeight(weights.value(raw));
            changed = true;
        } else
            qWarning() << "Unsupported font-weight ignored:" << raw;
    }

    if (style.contains("font-style")) {
        const QByteArray raw = style.value("font-style").toLower();
        if (raw == "italic" || raw == "oblique") {
            charFmt.setFontItalic(true);
            changed = true;
        } else if (raw == "normal") {
            charFmt.setFontItalic(false);
            changed = true;
        } else
            qWarning() << "Unsupported font-style ignored:" << raw;
    }

    if (style.contains("text-decoration")) {
        const QByteArray raw = style.value("text-decoration").toLower();
        if (raw == "underline") {
            charFmt.setFontUnderline(true);
            changed = true;
        } else if (raw.contains("none")) {
            charFmt.setFontUnderline(false);
            changed = true;
        } else
            qWarning() << "Unsupported text-decoration ignored:" << raw;
    }

    return changed;
}

CssDeclarations parseCss(const QByteArray &styleStr)
{
    CssDeclarations parsed;

    const QList<QByteArray> declList =
        styleStr.split(';');

    for (int i = 0; i < declList.size(); ++i) {
        const QByteArray declStr = declList.at(i).trimmed();

        if (declStr.isEmpty())
            continue;

        const qsizetype colonIndex = declStr.indexOf(':');

        if (colonIndex < 0)
            continue;

        const QByteArray name = declStr.left(colonIndex).trimmed().toLower();
        const QByteArray value = declStr.mid(colonIndex + 1).trimmed();

        parsed.insert(name, value);
    }

    return parsed;
}

static bool parseCssFloat(const QByteArray &raw, CssFloat &f)
{
    if (raw.isEmpty())
        return false;

    int pos = 0;
    while (pos < raw.size() && raw.at(pos) >= '0' && raw.at(pos) <= '9')
        ++pos;

    if (pos == 0)
        return false;

    if (pos < raw.size() && raw.at(pos) == '.') {
        ++pos;
        while (pos < raw.size() && raw.at(pos) >= '0' && raw.at(pos) <= '9')
            ++pos;
    }

    bool ok = false;
    const double value = raw.left(pos).toDouble(&ok);
    if (!ok)
        return false;

    const QByteArray unit = raw.mid(pos).trimmed();

    f.value = value;
    f.unit = unit;
    return true;
}
