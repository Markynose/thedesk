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
#include "m3circularprogress.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <cmath>

namespace {
    constexpr double TrackGap = 2.5, Waves = 10;
}

void M3CircularProgress::paint(QPainter& p, QPointF c, double r, double level, const QColor& active, const QColor& track, double amplitude, double phase) {
    level = qBound(0.0, level, 1.0);
    const double sweep = 360 * level, gap = qMin(sweep, qRadiansToDegrees((TrackGap + Stroke) / r));
    const QRectF box(c.x() - r, c.y() - r, 2 * r, 2 * r);
    p.save();
    p.setBrush(Qt::NoBrush);
    if (sweep + 2 * gap < 360) {
        p.setPen(QPen(track, Stroke, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(box, int((90 - sweep - gap) * 16), int(-(360 - sweep - 2 * gap) * 16));
    }
    p.setPen(QPen(active, Stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (sweep <= 0) {
        // nothing done yet: the track alone
    } else if (amplitude <= 0) {
        p.drawArc(box, 90 * 16, int(-qMax(sweep, 0.1) * 16));
    } else {
        QPainterPath path;
        const int steps = qMax(4, int(std::ceil(sweep / 3)));
        for (int i = 0; i <= steps; i++) {
            const double a = qDegreesToRadians(sweep * i / steps);
            const QPointF point = c + QPointF(std::sin(a), -std::cos(a)) * (r + amplitude * std::sin(Waves * a + phase));
            if (i == 0) path.moveTo(point);
            else path.lineTo(point);
        }
        p.drawPath(path);
    }
    p.restore();
}
