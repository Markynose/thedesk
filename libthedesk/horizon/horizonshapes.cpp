/****************************************
 *
 *   theDesk - living horizon, Material 3 Expressive style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "horizonshapes.h"
#include "horizonscene.h"

#include <QElapsedTimer>
#include <QHash>
#include <QTime>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <m3scheme.h>
#include <memory>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <tsettings.h>

namespace {
    using Getter = const m3shapes::RoundedPolygon& (*)();

    enum Role {
        PrimaryContainer,
        SecondaryContainer,
        TertiaryContainer,
        Primary,
        Secondary,
        Tertiary
    };

    // One arrangement, designed at 1366 x 768: big shapes half off the edges,
    // small bright accents, the middle left calm
    struct SlotDesign {
            QPointF centre;
            double radius;
            double rotation;
            Role role;
    };
    const SlotDesign layout[] = {
        {{120, 90},   210, 10,  PrimaryContainer  },
        {{1250, 120}, 150, 20,  TertiaryContainer },
        {{1190, 400}, 115, -35, SecondaryContainer},
        {{170, 400},  105, 15,  TertiaryContainer },
        {{980, 160},  44,  0,   Primary           },
        {{380, 290},  20,  0,   Tertiary          },
        {{760, 86},   28,  12,  Secondary         },
        {{1040, 330}, 34,  0,   Primary           },
    };
    constexpr int slotCount = int(std::size(layout));

    // Each scene puts its own shapes in the slots (indexes: HorizonScene::SceneId)
    const Getter sceneShapes[HorizonScene::SceneCount][slotCount] = {
        // DuskOcean
        {MaterialShapes::sunny, MaterialShapes::cookie12Sided, MaterialShapes::pill, MaterialShapes::cookie6Sided, MaterialShapes::flower, MaterialShapes::circle, MaterialShapes::diamond, MaterialShapes::softBurst},
        // Dune
        {MaterialShapes::cookie12Sided, MaterialShapes::sunny, MaterialShapes::bun, MaterialShapes::clover8Leaf, MaterialShapes::sunny, MaterialShapes::circle, MaterialShapes::pentagon, MaterialShapes::cookie4Sided},
        // AuroraNight
        {MaterialShapes::cookie9Sided, MaterialShapes::sunny, MaterialShapes::pill, MaterialShapes::clover4Leaf, MaterialShapes::softBurst, MaterialShapes::circle, MaterialShapes::gem, MaterialShapes::flower},
        // Ember
        {MaterialShapes::softBurst, MaterialShapes::cookie9Sided, MaterialShapes::pill, MaterialShapes::puffy, MaterialShapes::verySunny, MaterialShapes::circle, MaterialShapes::gem, MaterialShapes::clover4Leaf},
    };
    // Mirror the arrangement for some scenes so the times of day differ
    bool mirrored(int scene) {
        return scene == HorizonScene::DuskOcean || scene == HorizonScene::Ember;
    }
    double extraRotation(int scene) {
        return scene == HorizonScene::Dune || scene == HorizonScene::Ember ? 30 : 0;
    }

    // Scene seeds, as in m3/m3scheme.cpp
    const M3::Scheme& schemeFor(int scene) {
        static QHash<int, M3::Scheme> cache;
        auto it = cache.constFind(scene);
        if (it != cache.constEnd()) return *it;
        static const QColor seeds[HorizonScene::SceneCount] = {QColor(0xd2b292), QColor(0x95bad3), QColor(0x203626), QColor(0x995117)};
        return *cache.insert(scene, M3::Scheme::generate(seeds[qBound(0, scene, HorizonScene::SceneCount - 1)], true, M3::Variant::Vibrant));
    }

    // The shapes wear the session's own colours: the seed the bar, menus and
    // cursor use (the horizon's hue for this blend, or the chosen colour), as
    // a Vibrant dark scheme so they stay rich
    const M3::Scheme& liveScheme(int from, int to, double progress) {
        static M3::Config config;
        static QElapsedTimer age;
        if (!age.isValid() || age.elapsed() > 5000) {
            config = M3::Config::fromSettings();
            age.restart();
        }
        const bool horizon = config.seed.compare(QStringLiteral("horizon"), Qt::CaseInsensitive) == 0;
        // The time of day's seed, not the painted scene's: a pinned scene (the
        // lock screen's) still matches the clock and everything else
        Q_UNUSED(from)
        Q_UNUSED(to)
        Q_UNUSED(progress)
        const QColor seed = horizon ? M3::horizonSeed(QTime::currentTime()) : M3::seedFor(config);
        static QHash<QRgb, M3::Scheme> cache;
        auto it = cache.constFind(seed.rgb());
        if (it != cache.constEnd()) return *it;
        if (cache.size() > 64) cache.clear();
        return *cache.insert(seed.rgb(), M3::Scheme::generate(seed, true, M3::Variant::Vibrant));
    }

    QColor colour(const M3::Scheme& s, Role role) {
        switch (role) {
            case PrimaryContainer:
                return s.primaryContainer;
            case SecondaryContainer:
                return s.secondaryContainer;
            case TertiaryContainer:
                return s.tertiaryContainer;
            case Primary:
                return s.primary;
            case Secondary:
                return s.secondary;
            case Tertiary:
                return s.tertiary;
        }
        return s.primary;
    }

    // A slot's morph between two scenes' shapes, built once
    const m3shapes::Morph& morphFor(int from, int to, int slot) {
        static QHash<int, std::shared_ptr<m3shapes::Morph>> cache;
        int key = (from * HorizonScene::SceneCount + to) * slotCount + slot;
        auto it = cache.constFind(key);
        if (it != cache.constEnd()) return **it;
        auto morph = std::make_shared<m3shapes::Morph>(sceneShapes[from][slot](), sceneShapes[to][slot]());
        cache.insert(key, morph);
        return *morph;
    }

    // The overshooting entrance (an expressive spring, as a curve)
    double springIn(double t) {
        t = qBound(0.0, t, 1.0);
        return 1 - qExp(-6 * t) * qCos(9 * t);
    }
} // namespace

bool HorizonShapes::enabled() {
    static const bool on = [] {
        tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
        if (!settings.keysInGroup(QStringLiteral("Horizon")).contains(QStringLiteral("style"))) return true;
        return settings.value(QStringLiteral("Horizon/style")).toString() != QStringLiteral("classic");
    }();
    return on;
}

QColor HorizonShapes::sky(int scene) {
    return schemeFor(scene).surfaceContainerLowest;
}

QColor HorizonShapes::planet(int scene) {
    return schemeFor(scene).surfaceContainerLow;
}

QColor HorizonShapes::rim(int scene) {
    return schemeFor(scene).primary;
}

QColor HorizonShapes::glow(int scene) {
    return schemeFor(scene).primary;
}

void HorizonShapes::paint(QPainter* p, int from, int to, double progress, QSize screen, double shift, double intro, double opacity, QRectF quiet) {
    const double w = screen.width(), h = screen.height();
    const double sx = w / 1366.0, s = h / 768.0;
    const double t = qBound(0.0, progress, 1.0);
    const M3::Scheme& scheme = liveScheme(from, to, t);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    for (int i = 0; i < slotCount; i++) {
        const SlotDesign& slot = layout[i];
        auto place = [&](int scene) {
            QPointF c(slot.centre.x() * sx, slot.centre.y() * s);
            if (mirrored(scene)) c.setX(w - c.x());
            return c;
        };
        // Scenes with mirrored arrangements can't slide into each other (every
        // shape would meet in the middle half way through the blend): each
        // stays on its side, shrinking away just before the midpoint and
        // springing up on the other side just after it
        double swap = 1;
        QPointF centre;
        if (mirrored(from) != mirrored(to)) {
            centre = t < 0.5 ? place(from) : place(to);
            swap = qBound(0.0, qAbs(t - 0.5) / 0.04, 1.0);
            swap = swap * swap * (3 - 2 * swap);
        } else {
            centre = place(from) + (place(to) - place(from)) * t;
        }
        double rotation = slot.rotation + extraRotation(from) + (extraRotation(to) - extraRotation(from)) * t;
        double radius = slot.radius * s;

        // Parallax: big shapes drift a third as far as the planet, small
        // ones less; each turns with the slide, alternately
        double depth = slot.radius > 80 ? 0.33 : 0.18;
        centre.rx() += shift * depth;
        rotation += (i % 2 ? -1 : 1) * shift / w * 40;

        // Entrance: one after another, springing up to size
        double grow = springIn(intro * 1.5 - i * 0.07) * swap;
        if (grow <= 0.001) continue;

        QPainterPath path = from == to ? m3shapes::toPath(sceneShapes[from][i](), QRectF(-radius, -radius, 2 * radius, 2 * radius))
                                       : m3shapes::toPath(morphFor(from, to, i), float(t), QRectF(-radius, -radius, 2 * radius, 2 * radius));
        QTransform transform;
        transform.translate(centre.x(), centre.y());
        transform.rotate(rotation + (1 - grow) * 60);
        transform.scale(grow, grow);
        QPainterPath placed = transform.map(path);
        p->setOpacity(opacity * (quiet.isValid() && placed.intersects(quiet) ? 0.28 : 1.0));
        p->fillPath(placed, colour(scheme, slot.role));
    }
    p->restore();
}
