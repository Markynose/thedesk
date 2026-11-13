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
#ifndef HORIZONSHAPES_H
#define HORIZONSHAPES_H

#include "libthedesk_global.h"
#include <QColor>
#include <QRectF>
#include <QSize>

class QPainter;

/**
 * The horizon's Material 3 Expressive look (the default; Horizon/style=classic
 * in theDesk.conf brings back the painted scenes): dark, in the manner of
 * Pixel's "Hello" setup screen. Each scene is a dark M3 scheme seeded from
 * its colour; big MaterialShapes sit half off the edges with small bright
 * accents, over a flat dark planet with a thin rim.
 *
 * The shapes are painted live by HorizonView so they move with it: they drift
 * (parallax) and turn with desktop switches, spring in as the planet rises,
 * and morph slot by slot from one scene's shapes to the next's through the day.
 */
namespace HorizonShapes {
    LIBTHEDESK_EXPORT bool enabled();

    LIBTHEDESK_EXPORT QColor sky(int scene);    // the background
    LIBTHEDESK_EXPORT QColor planet(int scene); // the planet's surface
    LIBTHEDESK_EXPORT QColor rim(int scene);    // the rim line
    LIBTHEDESK_EXPORT QColor glow(int scene);   // the rim's glow

    // `progress` blends scene `from` into `to`; `shift` is the planet's
    // horizontal shift in px; `intro` runs 0 -> 1 while the planet rises
    // Shapes over `quiet` (a clock, say) fade back so it stays readable
    LIBTHEDESK_EXPORT void paint(QPainter* painter, int from, int to, double progress, QSize screen, double shift, double intro, double opacity = 1, QRectF quiet = QRectF());
} // namespace HorizonShapes

#endif // HORIZONSHAPES_H
