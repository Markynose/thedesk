/****************************************
 *
 *   theDesk - Material 3 colour and motion
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3CIRCULARPROGRESS_H
#define M3CIRCULARPROGRESS_H

#include <QColor>
#include <QPointF>

class QPainter;

/**
 * Compose's CircularProgressIndicator / CircularWavyProgressIndicator
 * (determinate) at bar size: the active arc clockwise from the top with round
 * caps, the track after a gap (gap + stroke) at both ends. With an amplitude
 * the arc's radius waves (phase in radians; pass a phase that grows to make
 * it drift). A whole-pixel centre and a radius of n + 0.5 keep the 3 px
 * stroke's edges on pixel edges.
 */
namespace M3CircularProgress {
    constexpr double Stroke = 3, WavyAmplitude = 0.8;
    void paint(QPainter& painter, QPointF centre, double radius, double level, const QColor& active, const QColor& track, double amplitude = 0, double phase = 0);
} // namespace M3CircularProgress

#endif // M3CIRCULARPROGRESS_H
