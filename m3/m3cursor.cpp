/****************************************
 *
 *   theDesk - Material 3 Expressive pointer
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3cursor.h"

#include "m3scheme.h"
#include "shapes/m3loadingindicator.h"
#include "shapes/roundedpolygon.h"
#include "shapes/shapepath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPathStroker>
#include <QGlyphRun>
#include <QRawFont>
#include <QTextLayout>
#include <QtMath>
#include <tsettings.h>

namespace {
    constexpr double keylineWidth = 1.5; // design units, outside the fill
    constexpr double shadowAlpha = 0.32;

    // Two box-blur passes over the alpha of a premultiplied black image
    void blurAlpha(QImage& image, int radius) {
        if (radius < 1) return;
        const int w = image.width(), h = image.height();
        std::vector<int> a(w * h), b(w * h);
        for (int y = 0; y < h; y++) {
            auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < w; x++) a[y * w + x] = qAlpha(line[x]);
        }
        for (int pass = 0; pass < 2; pass++) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int sum = 0, n = 0;
                    for (int k = -radius; k <= radius; k++) {
                        int xx = x + k;
                        if (xx < 0 || xx >= w) continue;
                        sum += a[y * w + xx];
                        n++;
                    }
                    b[y * w + x] = sum / n;
                }
            }
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int sum = 0, n = 0;
                    for (int k = -radius; k <= radius; k++) {
                        int yy = y + k;
                        if (yy < 0 || yy >= h) continue;
                        sum += b[yy * w + x];
                        n++;
                    }
                    a[y * w + x] = sum / n;
                }
            }
        }
        for (int y = 0; y < h; y++) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < w; x++) line[x] = qRgba(0, 0, 0, a[y * w + x]);
        }
    }

    QImage blank(int size) {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        return image;
    }

    // A soft drop shadow (1 dp down, ~1.5 dp blur), like a level-2 elevation
    void drawShadow(QImage& target, const QPainterPath& outline, const QTransform& transform) {
        const int size = target.width();
        const double unit = size / M3Cursor::DesignSize;
        QImage shadow = blank(size);
        {
            QPainter p(&shadow);
            p.setRenderHint(QPainter::Antialiasing);
            p.setTransform(transform * QTransform::fromTranslate(0, unit));
            p.fillPath(outline, QColor(0, 0, 0, qRound(255 * shadowAlpha)));
        }
        blurAlpha(shadow, qMax(1, qRound(unit)));
        QPainter p(&target);
        p.drawImage(0, 0, shadow);
    }

    // Fill with a keyline drawn outside it, over a shadow
    void drawInked(QImage& target, const QPainterPath& path, const QTransform& transform, const M3Cursor::Colours& colours) {
        QPainterPathStroker stroker;
        stroker.setWidth(keylineWidth * 2);
        stroker.setJoinStyle(Qt::RoundJoin);
        stroker.setCapStyle(Qt::RoundCap);
        QPainterPath outline = stroker.createStroke(path).united(path).simplified();

        drawShadow(target, outline, transform);
        QPainter p(&target);
        p.setRenderHint(QPainter::Antialiasing);
        p.setTransform(transform);
        p.fillPath(outline, colours.keyline);
        p.fillPath(path, colours.fill);
    }

    QTransform designTransform(int size) {
        double unit = size / M3Cursor::DesignSize;
        return QTransform::fromScale(unit, unit);
    }
} // namespace

bool M3Cursor::active() {
    static const bool isActive = [] {
        tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
        return settings.value(QStringLiteral("Platform/cursor")).toString() == QLatin1String(ThemeName) || qEnvironmentVariable("XCURSOR_THEME") == QLatin1String(ThemeName);
    }();
    return isActive;
}

int M3Cursor::nominalSize() {
    static const int size = [] {
        tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
        int configured = settings.value(QStringLiteral("Platform/cursorSize")).toInt();
        if (configured > 0) return configured;
        int env = qEnvironmentVariableIntValue("XCURSOR_SIZE");
        return env > 0 ? env : 24;
    }();
    return size;
}

QPainterPath M3Cursor::arrowPath() {
    // A tailless pointer: tip, down the left edge, the notch, out to the right
    // wing. Rounded like the MaterialShapes (CornerRounding per vertex).
    static const QPainterPath path = [] {
        using namespace m3shapes;
        // Clockwise on screen (y down), as graphics-shapes expects
        std::vector<float> vertices = {4.5f, 2.5f, 16.2f, 14.6f, 9.6f, 15.2f, 4.5f, 20.2f};
        std::vector<CornerRounding> rounding = {CornerRounding(0.9f), CornerRounding(1.4f), CornerRounding(0.8f), CornerRounding(1.4f)};
        return m3shapes::toPath(RoundedPolygon::fromVertices(vertices, Unrounded, rounding));
    }();
    return path;
}

QPointF M3Cursor::arrowHotspot() {
    const QRectF bounds = arrowPath().boundingRect();
    return QPointF(bounds.left(), bounds.top() + 0.6); // the rounded tip
}

QImage M3Cursor::renderArrow(int size, const Colours& colours, double scale) {
    QImage image = blank(size);
    if (scale <= 0.01) return image;
    const QPointF tip = arrowHotspot();
    QTransform t = QTransform::fromTranslate(-tip.x(), -tip.y()) * QTransform::fromScale(scale, scale) * QTransform::fromTranslate(tip.x(), tip.y()) * designTransform(size);
    drawInked(image, arrowPath(), t, colours);
    return image;
}

QPointF M3Cursor::textHotspot() {
    return QPointF(12, 12);
}

QImage M3Cursor::renderText(int size, const Colours& colours) {
    // A rounded I-beam: stem and short serifs as one stroked shape
    QPainterPath beam;
    beam.moveTo(9.5, 4.5);
    beam.quadTo(12, 4.5, 12, 6.5);
    beam.lineTo(12, 17.5);
    beam.quadTo(12, 19.5, 9.5, 19.5);
    beam.moveTo(14.5, 4.5);
    beam.quadTo(12, 4.5, 12, 6.5);
    beam.moveTo(12, 17.5);
    beam.quadTo(12, 19.5, 14.5, 19.5);
    QPainterPathStroker stroker;
    stroker.setWidth(1.8);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    QImage image = blank(size);
    drawInked(image, stroker.createStroke(beam).simplified(), designTransform(size), colours);
    return image;
}

QPointF M3Cursor::busyHotspot(bool withArrow) {
    return withArrow ? arrowHotspot() : QPointF(12, 12);
}

QImage M3Cursor::renderBusy(int size, const Colours& colours, qint64 ms, bool withArrow) {
    QImage image = withArrow ? renderArrow(size, colours) : blank(size);
    // The indicator sits in a white container (LoadingIndicator's contained
    // variant) so it reads on anything
    const QRectF box = withArrow ? QRectF(11.5, 11.5, 11.5, 11.5) : QRectF(2, 2, 20, 20);
    QPainterPath container;
    container.addEllipse(box);
    drawShadow(image, container, designTransform(size));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setTransform(designTransform(size));
    M3LoadingIndicator::paint(&p, box, ms, colours.fill, colours.keyline);
    return image;
}

namespace {
    // A filled Material Symbols Rounded glyph, `px` design units across its
    // longer side, centred in the 24 x 24 box and turned by `rotation`.
    // Empty when the font or the symbol isn't there (the name must shape to
    // a single glyph through the font's ligatures)
    QPainterPath symbolPath(const QString& name, double px, double rotation = 0, QPointF centre = QPointF(12, 12)) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(96);
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), 24);
        QTextLayout layout(name, font);
        layout.beginLayout();
        layout.createLine();
        layout.endLayout();
        const QList<QGlyphRun> runs = layout.glyphRuns();
        if (runs.size() != 1 || runs.first().glyphIndexes().size() != 1) return {};
        QPainterPath path = runs.first().rawFont().pathForGlyph(runs.first().glyphIndexes().first());
        path.setFillRule(Qt::WindingFill);
        const QRectF b = path.boundingRect();
        if (b.isEmpty()) return {};
        const double k = px / qMax(b.width(), b.height());
        QTransform t;
        t.translate(centre.x(), centre.y());
        t.rotate(rotation);
        t.scale(k, k);
        t.translate(-b.center().x(), -b.center().y());
        return t.map(path);
    }

    struct Extra {
            const char* name;
            const char* symbol;
            double px;
            double rotation;
            enum Kind { Glyph, Fingertip, Badge, Text } kind;
            QStringList aliases;
    };

    const QList<Extra>& extras() {
        static const QList<Extra> list = {
            {"pointer", "pan_tool_alt", 19, 0, Extra::Fingertip, {"hand1", "hand2", "pointing_hand", "e29285e634086352946a0e7090d73106", "9d800788f1b08800ae810202380a0822"}},
            {"grab", "pan_tool", 18, 0, Extra::Glyph, {"openhand", "5aca4d189052212118709018842178c0", "9141b49c8149039304290b508d208c40"}},
            {"grabbing", "back_hand", 18, 0, Extra::Glyph, {"closedhand", "dnd-none", "208530c400c041818281048008011002", "fcf21c00b30f7e3f83fe0dfd12e71cff"}},
            {"move", "open_with", 20, 0, Extra::Glyph, {"all-scroll", "fleur", "size_all", "dnd-move", "4498f0e0c1937ffe01fd06f973665830", "9081237383d90e509aa00f00170e968f"}},
            {"ew-resize", "arrow_range", 20, 0, Extra::Glyph, {"col-resize", "e-resize", "w-resize", "h_double_arrow", "sb_h_double_arrow", "size_hor", "left_side", "right_side", "split_h", "14fef782d02440884392942c11205230", "028006030e0e7ebffc7f7070c0600140"}},
            {"ns-resize", "arrow_range", 20, 90, Extra::Glyph, {"row-resize", "n-resize", "s-resize", "v_double_arrow", "sb_v_double_arrow", "size_ver", "top_side", "bottom_side", "split_v", "2870a09082c103050810ffdffffe0204", "00008160000006810000408080010102"}},
            {"nwse-resize", "arrow_range", 20, 45, Extra::Glyph, {"nw-resize", "se-resize", "size_fdiag", "bd_double_arrow", "top_left_corner", "bottom_right_corner", "c7088f0f3e6c8088236ef8e1e3e70000"}},
            {"nesw-resize", "arrow_range", 20, -45, Extra::Glyph, {"ne-resize", "sw-resize", "size_bdiag", "fd_double_arrow", "top_right_corner", "bottom_left_corner", "fcf1c3c7cd4491d801f1e1c78f100000"}},
            {"crosshair", "add", 20, 0, Extra::Glyph, {"cross", "tcross", "cell", "plus", "cross_reverse", "diamond_cross"}},
            {"not-allowed", "block", 18, 0, Extra::Glyph, {"no-drop", "dnd-no-drop", "forbidden", "circle", "crossed_circle", "03b6e0fcb3499374a867c041f52298f0"}},
            {"help", "question_mark", 9, 0, Extra::Badge, {"question_arrow", "whats_this", "left_ptr_help", "d9ce0ab605698f320427677b458ad60b", "5c6cd98b3f3ebcb1f9c7f1c204630408"}},
            {"zoom-in", "zoom_in", 19, 0, Extra::Glyph, {}},
            {"zoom-out", "zoom_out", 19, 0, Extra::Glyph, {}},
            {"copy", "add", 9, 0, Extra::Badge, {"dnd-copy", "1081e37283d90000800003c07f3ef6bf", "6407b0e94181790501fd1e167b474872", "b66166c04f8c3109214a4fbd64a50fc8"}},
            {"alias", "link", 9, 0, Extra::Badge, {"dnd-link", "link", "3085a0e285430894940527032f8b26df", "640fb0e74195791501fd1ed57b41487f", "a2a266d0498c3104214a47bd64ab0fc8"}},
            {"context-menu", "menu", 8, 0, Extra::Badge, {}},
            {"vertical-text", "", 0, 90, Extra::Text, {}},
            {"X_cursor", "close", 18, 0, Extra::Glyph, {"pirate"}},
        };
        return list;
    }
} // namespace

QStringList M3Cursor::extraCursorNames() {
    QStringList names;
    for (const Extra& e : extras()) names.append(QLatin1String(e.name));
    return names;
}

QImage M3Cursor::renderExtra(const QString& name, int size, const Colours& colours, QPointF* hotspot) {
    const Extra* extra = nullptr;
    for (const Extra& e : extras()) {
        if (name == QLatin1String(e.name)) extra = &e;
    }
    auto arrow = [&] {
        if (hotspot) *hotspot = arrowHotspot();
        return renderArrow(size, colours);
    };
    if (!extra) return arrow();

    switch (extra->kind) {
        case Extra::Text:
            {
                if (hotspot) *hotspot = QPointF(12, 12);
                QImage image = renderText(size, colours);
                QTransform turn;
                turn.rotate(extra->rotation);
                return image.transformed(turn, Qt::SmoothTransformation);
            }
        case Extra::Badge:
            {
                // The arrow with a round badge holding the symbol
                QImage image = renderArrow(size, colours);
                const QPainterPath glyph = symbolPath(QLatin1String(extra->symbol), extra->px, 0, QPointF(17.5, 17.5));
                if (glyph.isEmpty()) return arrow();
                QPainterPath badge;
                badge.addEllipse(QPointF(17.5, 17.5), 5.6, 5.6);
                drawInked(image, badge, designTransform(size), colours);
                QPainter p(&image);
                p.setRenderHint(QPainter::Antialiasing);
                p.setTransform(designTransform(size));
                p.fillPath(glyph, colours.keyline);
                if (hotspot) *hotspot = arrowHotspot();
                return image;
            }
        case Extra::Glyph:
        case Extra::Fingertip:
            {
                const QPainterPath glyph = symbolPath(QLatin1String(extra->symbol), extra->px, extra->rotation);
                if (glyph.isEmpty()) return arrow();
                QImage image = blank(size);
                drawInked(image, glyph, designTransform(size), colours);
                if (hotspot) {
                    *hotspot = QPointF(12, 12);
                    if (extra->kind == Extra::Fingertip) {
                        // The top of the raised finger
                        const QPolygonF outline = glyph.toFillPolygon();
                        QPointF top(12, 99);
                        for (const QPointF& point : outline) {
                            if (point.y() < top.y()) top = point;
                        }
                        *hotspot = top + QPointF(0, 0.5);
                    }
                }
                return image;
            }
    }
    return arrow();
}

M3Cursor::Colours M3Cursor::neutralColours() {
    return {QColor(0x1d, 0x1b, 0x20), QColor(Qt::white)};
}

M3Cursor::Colours M3Cursor::schemeColours() {
    M3::Scheme s = M3::currentScheme();
    return {s.primary, s.onPrimary};
}

QString M3Cursor::userIconsDir() {
    QString data = qEnvironmentVariable("XDG_DATA_HOME");
    if (data.isEmpty()) data = QDir::homePath() + QStringLiteral("/.local/share");
    return data + QStringLiteral("/icons");
}

// ---- Xcursor files (libXcursor's file format, little endian) ----------------

namespace {
    constexpr quint32 XcursorMagic = 0x72756358; // "Xcur"
    constexpr quint32 XcursorImageType = 0xfffd0002;
    constexpr int frameDelayMs = 40;

    struct Frame {
            QImage image;
            QPoint hotspot;
            quint32 delay;
            quint32 nominal;
    };

    void put(QByteArray& out, quint32 v) {
        char b[4] = {char(v & 0xff), char((v >> 8) & 0xff), char((v >> 16) & 0xff), char((v >> 24) & 0xff)};
        out.append(b, 4);
    }

    bool writeXcursor(const QString& path, const QList<Frame>& frames) {
        QByteArray out;
        const quint32 count = frames.size();
        put(out, XcursorMagic);
        put(out, 16);
        put(out, 0x10000);
        put(out, count);
        quint32 position = 16 + count * 12;
        for (const Frame& f : frames) {
            put(out, XcursorImageType);
            put(out, f.nominal);
            put(out, position);
            position += 36 + f.image.width() * f.image.height() * 4;
        }
        for (const Frame& f : frames) {
            put(out, 36);
            put(out, XcursorImageType);
            put(out, f.nominal);
            put(out, 1);
            put(out, f.image.width());
            put(out, f.image.height());
            put(out, f.hotspot.x());
            put(out, f.hotspot.y());
            put(out, f.delay);
            const QImage image = f.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            for (int y = 0; y < image.height(); y++) {
                auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
                for (int x = 0; x < image.width(); x++) put(out, line[x]);
            }
        }
        QFile file(path);
        if (!file.open(QFile::WriteOnly)) return false;
        return file.write(out) == out.size();
    }

    QPoint hotspotAt(QPointF design, int size) {
        return QPoint(qRound(design.x() * size / M3Cursor::DesignSize), qRound(design.y() * size / M3Cursor::DesignSize));
    }
} // namespace

bool M3Cursor::writeTheme(const QString& iconsDir, const Colours& colours, const QList<int>& sizes) {
    // Build next to the old theme, then swap, so nothing ever reads half a theme
    const QString target = iconsDir + QLatin1Char('/') + QLatin1String(ThemeName);
    const QString staging = target + QStringLiteral(".new");
    QDir(staging).removeRecursively();
    if (!QDir().mkpath(staging + QStringLiteral("/cursors"))) return false;

    QList<Frame> arrow, text, wait, progress;
    const qint64 loop = qint64(M3LoadingIndicator::MorphIntervalMillis) * 7; // one pass through the shapes
    for (int size : sizes) {
        arrow.append({renderArrow(size, colours), hotspotAt(arrowHotspot(), size), 0, quint32(size)});
        text.append({renderText(size, colours), hotspotAt(textHotspot(), size), 0, quint32(size)});
        for (qint64 ms = 0; ms < loop; ms += frameDelayMs) {
            wait.append({renderBusy(size, colours, ms, false), hotspotAt(busyHotspot(false), size), frameDelayMs, quint32(size)});
            progress.append({renderBusy(size, colours, ms, true), hotspotAt(busyHotspot(true), size), frameDelayMs, quint32(size)});
        }
    }
    const QString cursors = staging + QStringLiteral("/cursors/");
    if (!writeXcursor(cursors + QStringLiteral("default"), arrow) || !writeXcursor(cursors + QStringLiteral("text"), text) || !writeXcursor(cursors + QStringLiteral("wait"), wait) || !writeXcursor(cursors + QStringLiteral("progress"), progress)) {
        return false;
    }
    const QList<QPair<QString, QString>> aliases = {
        {QStringLiteral("left_ptr"), QStringLiteral("default")},
        {QStringLiteral("arrow"), QStringLiteral("default")},
        {QStringLiteral("top_left_arrow"), QStringLiteral("default")},
        {QStringLiteral("xterm"), QStringLiteral("text")},
        {QStringLiteral("ibeam"), QStringLiteral("text")},
        {QStringLiteral("watch"), QStringLiteral("wait")},
        {QStringLiteral("left_ptr_watch"), QStringLiteral("progress")},
        {QStringLiteral("half-busy"), QStringLiteral("progress")},
        {QStringLiteral("3ecb610c1bf2410f44200f48c40d3599"), QStringLiteral("progress")},
        {QStringLiteral("08e8e1c95fe2fc01f976f1e063a24ccd"), QStringLiteral("progress")},
    };
    for (const auto& [alias, file] : aliases) QFile::link(file, cursors + alias);

    for (const Extra& extra : extras()) {
        QList<Frame> frames;
        for (int size : sizes) {
            QPointF hotspot;
            QImage image = renderExtra(QLatin1String(extra.name), size, colours, &hotspot);
            frames.append({image, hotspotAt(hotspot, size), 0, quint32(size)});
        }
        if (!writeXcursor(cursors + QLatin1String(extra.name), frames)) return false;
        for (const QString& alias : extra.aliases) QFile::link(QLatin1String(extra.name), cursors + alias);
    }

    QFile index(staging + QStringLiteral("/index.theme"));
    if (!index.open(QFile::WriteOnly)) return false;
    index.write("[Icon Theme]\nName=theDesk-M3\nComment=theDesk Material 3 Expressive pointer\n"
                "Inherits=Adwaita\n");
    index.close();

    const QString old = target + QStringLiteral(".old");
    QDir(old).removeRecursively();
    if (QFileInfo::exists(target) && !QDir().rename(target, old)) return false;
    if (!QDir().rename(staging, target)) return false;
    QDir(old).removeRecursively();
    return true;
}
