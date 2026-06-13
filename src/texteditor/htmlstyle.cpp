#include "texteditor/htmlstyle.h"

#include <QDebug>
#include <QFont>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QRegularExpression>

bool applyHtmlStyle(const CssProperties &style, QTextCharFormat &charFormat)
{
    bool changed = false;

    if (style.contains(QStringLiteral("font-size"))) {
        const QString raw = style.value(QStringLiteral("font-size")).trimmed();
        static const QRegularExpression numberRe(QStringLiteral("^(\\d+)"));
        const auto match = numberRe.match(raw);
        if (match.hasMatch()) {
            charFormat.setFontPointSize(match.captured(1).toInt());
            changed = true;
        } else {
            static const QHash<QString, int> namedSizes {
                {QStringLiteral("xx-small"), 8},
                {QStringLiteral("x-small"), 9},
                {QStringLiteral("small"), 10},
                {QStringLiteral("medium"), 12},
                {QStringLiteral("large"), 14},
                {QStringLiteral("x-large"), 16},
                {QStringLiteral("xx-large"), 18},
            };
            const auto size = namedSizes.value(raw.toLower(), -1);
            if (size > 0) {
                charFormat.setFontPointSize(size);
                changed = true;
            } else
                qWarning() << "Unsupported font-size ignored:" << raw;
        }
    }

    if (style.contains(QStringLiteral("font-weight"))) {
        const QString raw = style.value(QStringLiteral("font-weight")).trimmed().toLower();
        static const QHash<QString, QFont::Weight> weights {
            {QStringLiteral("normal"), QFont::Normal},
            {QStringLiteral("bold"), QFont::Bold},
            {QStringLiteral("bolder"), QFont::Bold},
            {QStringLiteral("lighter"), QFont::Light},
            {QStringLiteral("100"), QFont::Thin},
            {QStringLiteral("200"), QFont::ExtraLight},
            {QStringLiteral("300"), QFont::Light},
            {QStringLiteral("400"), QFont::Normal},
            {QStringLiteral("500"), QFont::Medium},
            {QStringLiteral("600"), QFont::DemiBold},
            {QStringLiteral("700"), QFont::Bold},
            {QStringLiteral("800"), QFont::ExtraBold},
            {QStringLiteral("900"), QFont::Black},
        };
        if (weights.contains(raw)) {
            charFormat.setFontWeight(weights.value(raw));
            changed = true;
        } else
            qWarning() << "Unsupported font-weight ignored:" << raw;
    }

    if (style.contains(QStringLiteral("font-style"))) {
        const QString raw = style.value(QStringLiteral("font-style")).trimmed().toLower();
        if (raw == QStringLiteral("italic") || raw == QStringLiteral("oblique")) {
            charFormat.setFontItalic(true);
            changed = true;
        } else if (raw == QStringLiteral("normal")) {
            charFormat.setFontItalic(false);
            changed = true;
        } else
            qWarning() << "Unsupported font-style ignored:" << raw;
    }

    if (style.contains(QStringLiteral("text-decoration"))) {
        const QString raw = style.value(QStringLiteral("text-decoration")).trimmed().toLower();
        if (raw == QStringLiteral("underline")) {
            charFormat.setFontUnderline(true);
            changed = true;
        } else if (raw.contains(QStringLiteral("none"))) {
            charFormat.setFontUnderline(false);
            changed = true;
        } else
            qWarning() << "Unsupported text-decoration ignored:" << raw;
    }

    return changed;
}

CssProperties parseProperties(const QString &propertiesStr)
{
    CssProperties properties;

    const QStringList declarations =
        propertiesStr.split(QLatin1Char(';'), Qt::KeepEmptyParts);

    for (const QString &decl : declarations) {
        const QString property = decl.trimmed();

        if (property.isEmpty())
            continue;

        const qsizetype colonIndex = property.indexOf(QLatin1Char(':'));

        if (colonIndex < 0)
            continue;

        const QString name = property.left(colonIndex).trimmed();
        const QString value = property.mid(colonIndex + 1).trimmed();

        properties.insert(name, value);
    }

    return properties;
}
