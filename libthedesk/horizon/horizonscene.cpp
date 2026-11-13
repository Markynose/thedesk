/****************************************
 *
 *   theDesk - horizon scenes (splash + wallpapers)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "horizonscene.h"
#include "horizonshapes.h"

#include <horizonclock.h>

#include <QColor>
#include <QLinearGradient>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QtMath>

namespace {
    enum class Texture {
        Waves,
        Ripples,
        Soil,
        Night
    };

    struct Scene {
            QString name;
            QColor skyTop, skyHorizon;
            QColor surfaceRim, surfaceDeep;
            QColor rim, glow;
            Texture texture;
            bool stars = false;
            QColor haze = Qt::transparent; // band of light above the horizon
    };

    // Four original scenes, tinted toward Gruvbox
    const QList<Scene>& scenes() {
        static const QList<Scene> list = {
            {QStringLiteral("dusk-ocean"), QColor(0xc8b8a2), QColor(0xe9a46e), QColor(0x2f5f80), QColor(0x0b2135), QColor(0xffe6c0), QColor(0xf4b07a), Texture::Waves},
            {QStringLiteral("dune"), QColor(0x3a6f9e), QColor(0xa6c8de), QColor(0xd6a062), QColor(0x7a4420), QColor(0xffe8b8), QColor(0xf2d4a2), Texture::Ripples},
            {QStringLiteral("aurora-night"), QColor(0x05060b), QColor(0x140c1c), QColor(0x13261c), QColor(0x040706), QColor(0xc6f59a), QColor(0x8ec07c), Texture::Night, true, QColor(0xd3869b)},
            {QStringLiteral("ember"), QColor(0x160d08), QColor(0x3a2416), QColor(0x3e2316), QColor(0x0f0806), QColor(0xfabd2f), QColor(0xfe8019), Texture::Soil, true},
        };
        return list;
    }
} // namespace

double HorizonScene::Geometry::rimY(double x) const {
    double dx = x - centre.x();
    if (qAbs(dx) >= radius) return centre.y();
    return centre.y() - qSqrt(radius * radius - dx * dx);
}

HorizonScene::Geometry HorizonScene::geometryFor(QSize size) {
    Geometry g;
    const double w = size.width(), h = size.height();
    g.horizonY = h * 0.62;
    g.radius = qMax(w, h) * 1.45;
    g.centre = QPointF(w / 2, g.horizonY + g.radius);
    return g;
}

QString HorizonScene::sceneName(int scene) {
    return scenes().value(scene).name;
}

namespace {
    // Paint the requested layers of a scene. `canvas` is the target image
    // size; `screen` the screen size the scene is designed for (sets the
    // scale). `g` may have its centre moved horizontally.
    void paintScene(QPainter& p, const Scene& scene, const HorizonScene::Geometry& g, QSizeF canvas, QSizeF screen, int layers) {
    using namespace HorizonScene;
    const double w = canvas.width(), h = screen.height();
    const double scale = h / 768.0; // keep the look consistent across sizes
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath planet;
    planet.addEllipse(g.centre, g.radius, g.radius);

    QRandomGenerator rng(0x7de5c + static_cast<int>(scene.texture));

    if (layers & Sky) {
    // Sky
    QLinearGradient sky(0, 0, 0, g.horizonY);
    sky.setColorAt(0, scene.skyTop);
    sky.setColorAt(1, scene.skyHorizon);
    p.fillRect(QRectF(0, 0, w, h), sky);

    if (scene.stars) {
        p.setPen(Qt::NoPen);
        int count = int(160 * (w * h) / (1366.0 * 768.0));
        for (int i = 0; i < count; i++) {
            QPointF pt(rng.bounded(w), rng.bounded(g.horizonY));
            p.setBrush(QColor(255, 250, 235, 40 + rng.bounded(140)));
            double r = rng.bounded(0.5) + 0.4;
            p.drawEllipse(pt, r, r);
        }
    }
    }

    if (layers & Planet) {
    // Atmosphere: a halo that follows the curvature
    QRadialGradient halo(g.centre, g.radius + h * 0.35);
    double rimStop = g.radius / (g.radius + h * 0.35);
    QColor glow = scene.glow;
    glow.setAlpha(scene.texture == Texture::Night ? 150 : 110);
    QColor haze = scene.haze.alpha() ? scene.haze : scene.glow;
    haze.setAlpha(scene.haze.alpha() ? 70 : 0);
    halo.setColorAt(0, Qt::transparent);
    halo.setColorAt(rimStop - 0.0001, Qt::transparent);
    halo.setColorAt(rimStop, glow);
    halo.setColorAt(rimStop + (1 - rimStop) * 0.35, haze);
    halo.setColorAt(1, Qt::transparent);
    p.fillRect(QRectF(0, 0, w, h), halo);

    // Surface
    QRadialGradient surface(g.centre, g.radius);
    surface.setColorAt(0, scene.surfaceDeep);
    surface.setColorAt(qMax(0.0, 1 - (h * 0.45) / g.radius), scene.surfaceDeep);
    surface.setColorAt(1, scene.surfaceRim);
    p.fillPath(planet, surface);

    // Texture, clipped to the planet, denser toward the horizon
    p.save();
    p.setClipPath(planet);
    p.setBrush(Qt::NoBrush);
    switch (scene.texture) {
        case Texture::Waves:
        case Texture::Ripples:
            {
                bool ripples = scene.texture == Texture::Ripples;
                double depth = 0;
                double spacing = 1.5 * scale;
                while (depth < h - g.horizonY) {
                    double r = g.radius - depth;
                    int alpha = 10 + rng.bounded(22);
                    QColor c = rng.bounded(2) ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha + 10);
                    p.setPen(QPen(c, (1 + depth / (160 * scale)) * scale));
                    if (ripples) {
                        QPainterPath wave;
                        double phase = rng.bounded(6.28);
                        bool first = true;
                        for (double x = -10; x <= w + 10; x += 12 * scale) {
                            double dx = x - g.centre.x();
                            double y = g.centre.y() - qSqrt(qMax(0.0, r * r - dx * dx)) + qSin(x / ((40 + depth / 3) * scale) + phase) * (1.5 + depth / 60) * scale;
                            if (first) {
                                wave.moveTo(x, y);
                                first = false;
                            } else {
                                wave.lineTo(x, y);
                            }
                        }
                        p.drawPath(wave);
                    } else {
                        // Broken wave crests: short arcs at random positions
                        int crests = int(6 * w / 1366.0) + 1;
                        for (int i = 0; i < crests; i++) {
                            double x0 = rng.bounded(w);
                            double len = (30 + rng.bounded(120.0) * (1 + depth / (120 * scale))) * scale;
                            QPainterPath crest;
                            bool first = true;
                            for (double x = x0; x <= x0 + len; x += 6 * scale) {
                                double dx = x - g.centre.x();
                                double y = g.centre.y() - qSqrt(qMax(0.0, r * r - dx * dx));
                                if (first) {
                                    crest.moveTo(x, y);
                                    first = false;
                                } else {
                                    crest.lineTo(x, y);
                                }
                            }
                            p.drawPath(crest);
                        }
                    }
                    depth += spacing;
                    spacing *= 1.045; // perspective: lines spread out closer to the viewer
                }
                break;
            }
        case Texture::Soil:
        case Texture::Night:
            {
                p.setPen(Qt::NoPen);
                int count = int((scene.texture == Texture::Soil ? 5000 : 2200) * (w * h) / (1366.0 * 768.0));
                for (int i = 0; i < count; i++) {
                    double x = rng.bounded(w);
                    double y = g.horizonY + qPow(rng.bounded(1.0), 1.8) * (h - g.horizonY);
                    double s = (0.6 + (y - g.horizonY) / (140.0 * scale) + rng.bounded(1.0)) * scale;
                    int alpha = 10 + rng.bounded(30);
                    p.setBrush(rng.bounded(3) ? QColor(0, 0, 0, alpha) : QColor(255, 230, 200, alpha / 2));
                    p.drawEllipse(QPointF(x, y), s * 1.8, s * 0.7);
                }
                break;
            }
    }
    p.restore();
    }

    if (layers & Rim) {
    // Rim: a thin bright line along the horizon with a soft glow. Clear the
    // brush first: the rim is an open path and would otherwise be filled
    // with whatever brush the stars left behind.
    p.setBrush(Qt::NoBrush);
    QPainterPath rim;
    bool first = true;
    for (double x = 0; x <= w + 8; x += 8) {
        if (first) {
            rim.moveTo(x, g.rimY(x));
            first = false;
        } else {
            rim.lineTo(x, g.rimY(x));
        }
    }
    QColor rimGlow = scene.glow;
    rimGlow.setAlpha(45);
    p.setPen(QPen(rimGlow, 9 * scale, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(rim);
    QColor rimLine = scene.rim;
    rimLine.setAlpha(150);
    p.setPen(QPen(rimLine, 1.6 * scale));
    p.drawPath(rim);
    }
    }
} // namespace

namespace {
    // The M3 Expressive look (horizonshapes.h): flat and dark. The shapes
    // themselves are painted live by HorizonView.
    void paintExpressive(QPainter& p, int scene, const HorizonScene::Geometry& g, QSizeF canvas, QSizeF screen, int layers) {
        using namespace HorizonScene;
        const double w = canvas.width(), h = screen.height();
        const double scale = h / 768.0;
        p.setRenderHint(QPainter::Antialiasing);
        if (layers & Sky) p.fillRect(QRectF(0, 0, w, h), HorizonShapes::sky(scene));
        if (layers & Planet) {
            QPainterPath planet;
            planet.addEllipse(g.centre, g.radius, g.radius);
            p.fillPath(planet, HorizonShapes::planet(scene));
            // One flat inner band for depth
            QPainterPath core;
            double inner = g.radius - 44 * scale;
            core.addEllipse(g.centre, inner, inner);
            QColor deep = HorizonShapes::sky(scene);
            QColor surface = HorizonShapes::planet(scene);
            p.fillPath(core, QColor::fromRgbF((deep.redF() + surface.redF()) / 2, (deep.greenF() + surface.greenF()) / 2, (deep.blueF() + surface.blueF()) / 2));
        }
        if (layers & Rim) {
            p.setBrush(Qt::NoBrush);
            QColor glow = HorizonShapes::glow(scene);
            glow.setAlphaF(0.18);
            p.setPen(QPen(glow, 8 * scale));
            p.drawEllipse(g.centre, g.radius, g.radius);
            p.setPen(QPen(HorizonShapes::rim(scene), 2.5 * scale));
            p.drawEllipse(g.centre, g.radius, g.radius);
        }
    }
} // namespace

QImage HorizonScene::render(int sceneIndex, QSize size, qreal dpr, int layers) {
    const Scene& scene = scenes().at(qBound(0, sceneIndex, int(scenes().count()) - 1));
    QImage image(size * dpr, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter p(&image);
    if (HorizonShapes::enabled()) {
        paintExpressive(p, sceneIndex, geometryFor(size), size, size, layers);
    } else {
        paintScene(p, scene, geometryFor(size), size, size, layers);
    }
    p.end();
    return image;
}

QImage HorizonScene::renderPlanet(int sceneIndex, QSize screenSize, int margin, qreal dpr, bool withRim) {
    const Scene& scene = scenes().at(qBound(0, sceneIndex, int(scenes().count()) - 1));
    QSize canvas(screenSize.width() + 2 * margin, screenSize.height());
    Geometry g = geometryFor(screenSize);
    g.centre.setX(canvas.width() / 2.0);

    QImage image(canvas * dpr, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter p(&image);
    if (HorizonShapes::enabled()) {
        paintExpressive(p, sceneIndex, g, canvas, screenSize, Planet | (withRim ? Rim : 0));
    } else {
        paintScene(p, scene, g, canvas, screenSize, Planet | (withRim ? Rim : 0));
    }
    p.end();
    return image;
}

QColor HorizonScene::rimColour(int scene) {
    if (HorizonShapes::enabled()) return HorizonShapes::rim(scene);
    return scenes().value(scene).rim;
}

QColor HorizonScene::glowColour(int scene) {
    if (HorizonShapes::enabled()) return HorizonShapes::glow(scene);
    return scenes().value(scene).glow;
}

HorizonScene::Blend HorizonScene::blendForTime(QTime time) {
    // The keyframes live in m3/horizonclock.cpp, shared with the M3 colour seed
    HorizonClock::Blend clock = HorizonClock::blendForTime(time);
    Blend blend;
    blend.from = clock.from;
    blend.to = clock.to;
    blend.progress = clock.progress;
    return blend;
}
