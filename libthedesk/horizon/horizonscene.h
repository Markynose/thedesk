/****************************************
 *
 *   theDesk - horizon scenes (splash, wallpaper, lock screen)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef HORIZONSCENE_H
#define HORIZONSCENE_H

#include "libthedesk_global.h"
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QTime>

/**
 * Procedurally drawn "planet horizon" scenes: a curved horizon under a soft
 * sky with a glowing rim. No text is drawn here.
 *
 * Scenes can be rendered whole, or as separate layers so the planet can be
 * moved independently of the sky (HorizonView slides it between desktops).
 */
namespace HorizonScene {
    enum SceneId {
        DuskOcean = 0,
        Dune,
        AuroraNight,
        Ember,
        SceneCount
    };

    enum Layer {
        Sky = 0x1,    // sky gradient + stars
        Planet = 0x2, // atmosphere halo + surface + texture (transparent elsewhere)
        Rim = 0x4,    // thin glowing horizon line
        AllLayers = Sky | Planet | Rim
    };

    struct LIBTHEDESK_EXPORT Geometry {
            QPointF centre; // centre of the planet circle
            double radius = 0;
            double horizonY = 0; // top of the planet (horizon at the apex)

            double rimY(double x) const; // y of the horizon at x
    };

    struct LIBTHEDESK_EXPORT Blend {
            int from = DuskOcean;
            int to = DuskOcean;
            double progress = 0; // 0 = from, 1 = to
    };

    // Geometry for a screen of the given size (apex centred horizontally)
    LIBTHEDESK_EXPORT Geometry geometryFor(QSize screenSize);
    LIBTHEDESK_EXPORT QString sceneName(int scene); // "dusk-ocean", "dune", "aurora-night", "ember"
    LIBTHEDESK_EXPORT QColor rimColour(int scene);
    LIBTHEDESK_EXPORT QColor glowColour(int scene);

    // Which scenes to show at a time of day: night aurora, ocean at dawn,
    // dune by day, ocean again at golden hour, ember after sunset.
    LIBTHEDESK_EXPORT Blend blendForTime(QTime time);

    // Whole scene at screen size
    LIBTHEDESK_EXPORT QImage render(int scene, QSize size, qreal devicePixelRatio = 1, int layers = AllLayers);

    // Planet layer for a screen, widened by `margin` px on each side so it
    // can be shifted horizontally. The apex sits at the centre of the image.
    LIBTHEDESK_EXPORT QImage renderPlanet(int scene, QSize screenSize, int margin, qreal devicePixelRatio = 1, bool withRim = false);
} // namespace HorizonScene

#endif // HORIZONSCENE_H
