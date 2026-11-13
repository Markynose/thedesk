/****************************************
 *
 *   theDesk - boot theme generator
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/

// Renders the boot path's art from the same horizon scene, shapes and M3
// scheme as the session, so boot looks like theDesk before theDesk runs:
//
//   thedesk-boot-theme plymouth OUTDIR [--scene NAME]
//     the Plymouth theme "thedesk" (see writePlymouth)
//
//   thedesk-boot-theme limine OUTDIR [--size WxH] [--scene NAME]
//     thedesk.png         the menu's wallpaper: horizon, shapes, the mark
//                         and a card that Limine's terminal sits inside
//     roboto-flex.f16     an 8x16 CP437 bitmap font cut from Roboto Flex
//     theme.conf          Limine's global options in the scheme's colours
//
// Limine (or Plymouth) can't compute colours at boot, so they're baked here
// from one scene: aurora-night by default.

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTextStream>
#include <horizon/horizonscene.h>
#include <horizon/horizonshapes.h>
#include <m3scheme.h>
#include <shapes/m3loadingindicator.h>
#include <shapes/materialshapes.h>
#include <QtMath>
#include <shapes/shapepath.h>

namespace {
    // The scene seeds, as in libthedesk/horizon/horizonshapes.cpp
    M3::Scheme schemeFor(int scene) {
        static const QColor seeds[HorizonScene::SceneCount] = {QColor(0xd2b292), QColor(0x95bad3), QColor(0x203626), QColor(0x995117)};
        return M3::Scheme::generate(seeds[qBound(0, scene, HorizonScene::SceneCount - 1)], true, M3::Variant::Vibrant);
    }

    QString hex(const QColor& c) { return c.name().mid(1); }

    // ---- CP437 font
    const char16_t cp437High[128] = {
        0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
        0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
        0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
        0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
        0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
        0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0};

    QChar cp437(int i) {
        if (i < 32 || i == 127) return QChar(u' ');
        if (i < 127) return QChar(i);
        return QChar(cp437High[i - 128]);
    }

    // 256 glyphs of 16 rows, one byte a row, leftmost pixel in the top bit.
    // Letters are Roboto Flex, centred in the 8 px cell (wide ones squeezed);
    // box drawing comes from the font's fallback so lines join up
    QByteArray limineFont() {
        QByteArray out(256 * 16, 0);
        for (int i = 0; i < 256; i++) {
            const QChar c = cp437(i);
            QImage img(8, 16, QImage::Format_Grayscale8);
            img.fill(0);
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            const bool box = c.unicode() >= 0x2500 && c.unicode() <= 0x259F;
            if (box) {
                QFont f(QStringLiteral("monospace"));
                f.setPixelSize(16);
                p.setFont(f);
                p.setPen(Qt::white);
                p.drawText(QRect(0, 0, 8, 16), Qt::AlignCenter, c);
            } else {
                QFont f(QStringLiteral("Roboto Flex"));
                f.setPixelSize(13);
                f.setVariableAxis(QFont::Tag("opsz"), 13);
                f.setVariableAxis(QFont::Tag("wdth"), 100);
                f.setVariableAxis(QFont::Tag("wght"), 560);
                QPainterPath path;
                path.setFillRule(Qt::WindingFill);
                path.addText(0, 0, f, c);
                const QRectF b = path.boundingRect();
                const double sx = b.width() > 7.6 ? 7.6 / b.width() : 1;
                QTransform t;
                t.translate(4 - b.center().x() * sx, 12);
                t.scale(sx, 1);
                p.fillPath(t.map(path), Qt::white);
            }
            p.end();
            for (int y = 0; y < 16; y++) {
                uchar row = 0;
                for (int x = 0; x < 8; x++)
                    if (qGray(img.pixel(x, y)) > 100) row |= 0x80 >> x;
                out[i * 16 + y] = char(row);
            }
        }
        return out;
    }

    QPainterPath glyphs(const QString& text, double px, double wght, double wdth) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(int(px));
        font.setVariableAxis(QFont::Tag("opsz"), float(qMin(px, 144.0)));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        path.addText(0, 0, font, text);
        return path;
    }

    // The Gateway mark: Sunny in TertiaryContainer with a heavy, wide "d"
    void paintMark(QPainter& p, const QRectF& r, const M3::Scheme& s) {
        p.fillPath(m3shapes::toPath(MaterialShapes::sunny(), r), s.tertiaryContainer);
        QPainterPath d = glyphs(QStringLiteral("d"), r.height() * 0.62, 1000, 151);
        p.fillPath(d.translated(r.center() - d.boundingRect().center()), s.onTertiaryContainer);
    }

    // Limine draws its terminal inside term_margin on every side
    int limineMargin(QSize size) {
        return qMax(16, (size.height() - 17 * 16) / 2); // seventeen rows of text
    }

    bool writeLimine(const QString& dir, QSize size, int scene) {
        QDir().mkpath(dir);
        const M3::Scheme s = schemeFor(scene);

        QImage img = HorizonScene::render(scene, size);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        HorizonShapes::paint(&p, scene, scene, 0, size, 0, 1);

        const int margin = limineMargin(size);
        const QRectF terminal(margin, margin, size.width() - 2 * margin, size.height() - 2 * margin);
        QPainterPath card;
        card.addRoundedRect(terminal.adjusted(-24, -24, 24, 24), 28, 28);
        QColor cardColour = s.surfaceContainer;
        cardColour.setAlphaF(0.97);
        p.fillPath(card, cardColour);

        // The mark and the word, centred over the card
        const double markSize = qMin(72.0, margin * 0.4);
        QPainterPath word = glyphs(QStringLiteral("theDesk"), markSize * 0.72, 800, 120);
        const double total = markSize + 12 + word.boundingRect().width();
        const double left = size.width() / 2.0 - total / 2;
        const double cy = (margin - 24) / 2.0 + 8;
        paintMark(p, QRectF(left, cy - markSize / 2, markSize, markSize), s);
        p.fillPath(word.translated(QPointF(left + markSize + 12, cy) - QPointF(word.boundingRect().left(), word.boundingRect().center().y())), s.onSurface);
        p.end();
        if (!img.save(dir + QStringLiteral("/thedesk.png"))) return false;

        QFile font(dir + QStringLiteral("/roboto-flex.f16"));
        if (!font.open(QFile::WriteOnly)) return false;
        font.write(limineFont());
        font.close();

        // Text is OnSurface on the card (the terminal's background is fully
        // transparent); the selected entry inverts to an OnSurface band
        QFile conf(dir + QStringLiteral("/theme.conf"));
        if (!conf.open(QFile::WriteOnly)) return false;
        QTextStream t(&conf);
        const QString palette = QStringList{hex(s.surfaceContainer), hex(s.error), hex(s.primary), hex(s.tertiary), hex(s.secondary), hex(s.tertiary), hex(s.primary), hex(s.onSurfaceVariant)}.join(';');
        const QString bright = QStringList{hex(s.outline), hex(s.error), hex(s.primary), hex(s.tertiary), hex(s.secondary), hex(s.tertiary), hex(s.primary), hex(s.onSurface)}.join(';');
        t << "# Generated by thedesk-boot-theme (scene " << HorizonScene::sceneName(scene) << ")\n"
          << "interface_resolution: " << size.width() << 'x' << size.height() << '\n'
          << "wallpaper: boot():/EFI/limine/thedesk.png\n"
          << "wallpaper_style: stretched\n"
          << "backdrop: " << hex(HorizonShapes::sky(scene)) << '\n'
          << "interface_branding: Boot\n"
          << "interface_branding_colour: " << hex(s.primary) << '\n'
          << "interface_help_colour: " << hex(s.outline) << '\n'
          << "interface_help_colour_bright: " << hex(s.primary) << '\n'
          << "term_font: boot():/EFI/limine/roboto-flex.f16\n"
          << "term_font_size: 8x16\n"
          << "term_font_spacing: 0\n"
          << "term_margin: " << margin << '\n'
          << "term_margin_gradient: 0\n"
          << "term_background: FF000000\n"
          << "term_foreground: " << hex(s.onSurface) << '\n'
          << "term_background_bright: " << hex(s.secondaryContainer) << '\n'
          << "term_foreground_bright: " << hex(s.onSecondaryContainer) << '\n'
          << "term_palette: " << palette << '\n'
          << "term_palette_bright: " << bright << '\n';
        return true;
    }
    // ---- Plymouth: the mark morphing through the loading indicator's shapes
    // over an M3E wavy progress bar (stage 10 option 1). Plymouth's script
    // plugin can crop and rotate images but not draw, so the pieces are
    // rendered here and put together by thedesk.script:
    //   mark-N.png   the morph springs, 7 morphs x 15 steps of 20 ms, each
    //                at its own spring rotation only; the script adds the
    //                quarter-turn steps and the slow spin (Image.Rotate)
    //   d.png        the "d", which stays upright
    //   wave.png     a long wave, cropped every frame to scroll it (back
    //                toward the start) and to the boot progress
    //   track.png    the rest of the track, with the stop dot at its end
    QImage canvas(QSize size) {
        QImage img(size, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        return img;
    }

    bool savePng(const QImage& img, const QString& path) { return img.save(path, "PNG"); }

    QString rgbF(const QColor& c) {
        return QStringLiteral("%1, %2, %3").arg(c.redF(), 0, 'f', 3).arg(c.greenF(), 0, 'f', 3).arg(c.blueF(), 0, 'f', 3);
    }

    bool writePlymouth(const QString& dir, int scene) {
        QDir().mkpath(dir);
        const M3::Scheme s = schemeFor(scene);
        const QColor background = HorizonShapes::sky(scene);
        const int morphs = 7, steps = 15, stepMs = 20, markCanvas = 160;
        const double waveLength = 40, amplitude = 3, stroke = 4, barWidth = 240;
        const int stripHeight = 16;

        // The mark's shape: the indeterminate indicator in a 156 px container
        // around the 128 px mark, as theDesk draws it
        for (int m = 0; m < morphs; m++) {
            for (int k = 0; k < steps; k++) {
                const qint64 elapsed = qint64(m) * M3LoadingIndicator::MorphIntervalMillis + k * stepMs;
                const auto state = M3LoadingIndicator::indeterminateState(elapsed);
                const QRectF box(2, 2, markCanvas - 4, markCanvas - 4);
                QPainterPath path = M3LoadingIndicator::indeterminatePath(box, elapsed);
                // Keep only the spring's own turn; the script adds the rest
                QTransform back;
                back.translate(box.center().x(), box.center().y());
                back.rotate(-(state.rotation - state.morphProgress * 90));
                back.translate(-box.center().x(), -box.center().y());
                QImage img = canvas(QSize(markCanvas, markCanvas));
                QPainter p(&img);
                p.setRenderHint(QPainter::Antialiasing);
                p.fillPath(back.map(path), s.tertiaryContainer);
                p.end();
                if (!savePng(img, QStringLiteral("%1/mark-%2.png").arg(dir).arg(m * steps + k))) return false;
            }
        }

        {
            // The "d" (smaller than it was: 0.48 of the mark, not 0.62)
            QImage img = canvas(QSize(128, 128));
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath d = glyphs(QStringLiteral("d"), 128 * 0.48, 1000, 151);
            p.fillPath(d.translated(QPointF(64, 64) - d.boundingRect().center()), s.onTertiaryContainer);
            p.end();
            if (!savePng(img, dir + QStringLiteral("/d.png"))) return false;
        }

        // y = amplitude * sin(2 pi x / wavelength), one wavelength longer than
        // the bar so any crop offset fits; wave.png at full amplitude, then
        // wave-1..3 flatter (the bar irons flat as boot finishes, as M3's
        // wavy progress does when it's done)
        for (int level = 0; level < 4; level++) {
            const double a = amplitude * (1 - level / 3.0);
            QImage img = canvas(QSize(int(barWidth + waveLength) + 1, stripHeight));
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath wave;
            for (double x = -4; x <= img.width() + 4; x += 0.5) {
                const double y = stripHeight / 2.0 + a * qSin(x / waveLength * 2 * M_PI);
                if (x == -4) wave.moveTo(x, y);
                else wave.lineTo(x, y);
            }
            p.setPen(QPen(s.primary, stroke, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
            p.drawPath(wave);
            p.end();
            if (!savePng(img, level == 0 ? dir + QStringLiteral("/wave.png") : QStringLiteral("%1/wave-%2.png").arg(dir).arg(level))) return false;
        }

        {
            // The track to the bar's end (flat at the left: it's cropped
            // there and capped with track-dot.png), then the stop dot
            QImage img = canvas(QSize(int(barWidth) + 4, stripHeight));
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(s.secondaryContainer, stroke, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(-4, stripHeight / 2.0), QPointF(barWidth, stripHeight / 2.0));
            p.setPen(Qt::NoPen);
            p.setBrush(s.primary);
            p.drawEllipse(QPointF(barWidth, stripHeight / 2.0), 2, 2);
            p.end();
            if (!savePng(img, dir + QStringLiteral("/track.png"))) return false;
        }

        auto dot = [&](const QColor& c, const QString& name) {
            QImage img = canvas(QSize(int(stroke) + 2, int(stroke) + 2));
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawEllipse(QRectF(1, 1, stroke, stroke));
            p.end();
            return savePng(img, dir + QStringLiteral("/") + name);
        };
        if (!dot(s.primary, QStringLiteral("wave-dot.png")) || !dot(s.secondaryContainer, QStringLiteral("track-dot.png"))) return false;

        {
            // The password field (disk encryption prompts): a 56 dp pill
            QImage img = canvas(QSize(320, 56));
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath pill;
            pill.addRoundedRect(QRectF(0, 0, 320, 56), 28, 28);
            p.fillPath(pill, s.surfaceContainerHighest);
            p.end();
            if (!savePng(img, dir + QStringLiteral("/field.png"))) return false;
        }

        QFile theme(dir + QStringLiteral("/thedesk.plymouth"));
        if (!theme.open(QFile::WriteOnly)) return false;
        QTextStream(&theme) << "[Plymouth Theme]\n"
                               "Name=theDesk\n"
                               "Description=theDesk's mark morphing through the M3 Expressive loading indicator's shapes, over a wavy progress bar\n"
                               "ModuleName=script\n"
                               "Font=Roboto Flex 12\n\n"
                               "[script]\n"
                               "ImageDir=/usr/share/plymouth/themes/thedesk\n"
                               "ScriptFile=/usr/share/plymouth/themes/thedesk/thedesk.script\n";
        theme.close();

        QFile in(QStringLiteral(":/thedesk.script"));
        QFile out(dir + QStringLiteral("/thedesk.script"));
        if (!in.open(QFile::ReadOnly) || !out.open(QFile::WriteOnly)) return false;
        QString script = QString::fromUtf8(in.readAll());
        script.replace(QStringLiteral("@BACKGROUND@"), rgbF(background));
        // The entrance: an expressive spring (damping 0.5, stiffness 300),
        // sampled every 20 ms for 1.2 s (Plymouth's script has no exp())
        {
            QStringList values;
            for (int i = 0; i < 60; i++) {
                const double t = i * 0.02, w = std::sqrt(300.0), z = 0.5, wd = w * std::sqrt(1 - z * z);
                const double v = 1 - std::exp(-z * w * t) * (std::cos(wd * t) + z * w / wd * std::sin(wd * t));
                values.append(QStringLiteral("ENTRANCE[%1] = %2;").arg(i).arg(v, 0, 'f', 4));
            }
            script.replace(QStringLiteral("@ENTRANCE@"), values.join(QLatin1Char('\n')));
        }
        script.replace(QStringLiteral("@ON_SURFACE@"), rgbF(s.onSurface));
        script.replace(QStringLiteral("@ON_SURFACE_VARIANT@"), rgbF(s.onSurfaceVariant));
        out.write(script.toUtf8());
        return true;
    }
} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qunsetenv("QT_QPA_PLATFORMTHEME"); // no desktop theme plugins needed here
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Renders theDesk's boot themes (Limine, Plymouth) from the horizon and its M3 scheme"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("target"), QStringLiteral("limine or plymouth"));
    parser.addPositionalArgument(QStringLiteral("outdir"), QStringLiteral("Where to write the files"));
    QCommandLineOption sizeOption(QStringLiteral("size"), QStringLiteral("Screen size, WxH (default 1366x768)"), QStringLiteral("size"), QStringLiteral("1366x768"));
    QCommandLineOption sceneOption(QStringLiteral("scene"), QStringLiteral("Horizon scene to bake (default aurora-night)"), QStringLiteral("scene"), QStringLiteral("aurora-night"));
    parser.addOption(sizeOption);
    parser.addOption(sceneOption);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2 || (args.first() != QStringLiteral("limine") && args.first() != QStringLiteral("plymouth"))) parser.showHelp(1);

    const QStringList wh = parser.value(sizeOption).split('x');
    const QSize size(wh.value(0).toInt(), wh.value(1).toInt());
    if (size.width() < 640 || size.height() < 480) {
        QTextStream(stderr) << "Bad --size\n";
        return 1;
    }
    int scene = -1;
    for (int i = 0; i < HorizonScene::SceneCount; i++) {
        if (HorizonScene::sceneName(i) == parser.value(sceneOption)) scene = i;
    }
    if (scene < 0) {
        QTextStream(stderr) << "Unknown --scene\n";
        return 1;
    }

    if (args.first() == QStringLiteral("plymouth")) return writePlymouth(args.at(1), scene) ? 0 : 1;
    return writeLimine(args.at(1), size, scene) ? 0 : 1;
}
