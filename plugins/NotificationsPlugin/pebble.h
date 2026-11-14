/****************************************
 *
 *   theDesk - notifications as pebbles: every app is a MaterialShape
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef PEBBLE_H
#define PEBBLE_H

#include <Applications/application.h>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <m3motion.h>
#include <m3scheme.h>
#include <m3themedicon.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>

namespace Pebble {
    // One spring, stepped by its owner's frame timer
    struct Spring {
            double value = 0, velocity = 0, target = 0;
            M3Motion::Token token = M3Motion::SpatialDefault;
            double eps = 0.002; // settled within this (and still within ten of it a second): for 0-1 values; degrees and pixels take more
            Spring(double v = 0, M3Motion::Token t = M3Motion::SpatialDefault) :
                value(v), target(v), token(t) {}
            // true while it still moves
            bool step(double dt) {
                const M3Motion::SpringState s = M3Motion::stepSpring(value, velocity, target, M3Motion::spring(token), dt);
                value = s.value;
                velocity = s.velocity;
                if (qAbs(value - target) < eps && qAbs(velocity) < eps * 10) {
                    value = target;
                    velocity = 0;
                    return false;
                }
                return true;
            }
            void snap(double v) {
                value = target = v;
                velocity = 0;
            }
    };

    // An app is known by its desktop entry, else by the name it gave
    inline QString key(const ApplicationPointer& app) {
        if (!app) return {};
        const QString entry = app->desktopEntry();
        return entry.isEmpty() ? app->getProperty(QStringLiteral("Name")).toString() : entry;
    }

    // The same app always gets the same shape and colour (a checksum, not
    // qHash: that one is seeded per run)
    inline quint16 hash(const QString& key) {
        return qChecksum(key.toUtf8());
    }
    inline const m3shapes::RoundedPolygon& shape(const QString& key) {
        using Shape = const m3shapes::RoundedPolygon& (*)();
        static const Shape shapes[] = {MaterialShapes::cookie9Sided, MaterialShapes::clover4Leaf, MaterialShapes::sunny, MaterialShapes::cookie7Sided,
            MaterialShapes::flower, MaterialShapes::cookie6Sided, MaterialShapes::clover8Leaf, MaterialShapes::puffyDiamond};
        return shapes[hash(key) % 8]();
    }
    inline void colours(const QString& key, const M3::Scheme& s, QColor* bg, QColor* fg) {
        switch (hash(key) / 8 % 3) {
            case 0:
                *bg = s.primaryContainer;
                *fg = s.onPrimaryContainer;
                break;
            case 1:
                *bg = s.secondaryContainer;
                *fg = s.onSecondaryContainer;
                break;
            default:
                *bg = s.tertiaryContainer;
                *fg = s.onTertiaryContainer;
        }
    }

    inline QFont flex(double px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }
    inline QFont symbols(double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qMax(1, qRound(px)));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        return font;
    }

    // The app's icon as it sits in its shape: a duotone in the shape's colours
    inline QImage glyph(const ApplicationPointer& app, int side, qreal dpr, const QColor& bg, const QColor& fg) {
        QIcon icon = app ? QIcon::fromTheme(app->getProperty(QStringLiteral("Icon")).toString()) : QIcon();
        QImage image = icon.pixmap(QSize(side, side), dpr).toImage();
        if (image.isNull()) {
            image = QImage(QSize(side, side) * dpr, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(dpr);
            image.fill(Qt::transparent);
            QPainter p(&image);
            p.setFont(symbols(side * 0.9));
            p.setPen(fg);
            p.drawText(QRectF(0, 0, side, side), Qt::AlignCenter, QStringLiteral("notifications"));
            return image;
        }
        if (M3ThemedIcon::enabled()) image = M3ThemedIcon::glyph(image, bg, fg);
        image.setDevicePixelRatio(dpr);
        return image;
    }

    // A shape (or a morph) at `centre`, `size` across, turned `rotation` degrees
    inline QPainterPath shapePath(const QPainterPath& normalised, QPointF centre, double size, double rotation) {
        QTransform t;
        t.translate(centre.x(), centre.y());
        t.rotate(rotation);
        return t.map(m3shapes::fitToRect(normalised, QRectF(-size / 2, -size / 2, size, size)));
    }
    inline void drawCentred(QPainter& p, QPointF centre, const QImage& image, double side) {
        if (image.isNull() || side < 1) return;
        p.drawImage(QRectF(centre.x() - side / 2, centre.y() - side / 2, side, side), image);
    }
} // namespace Pebble

#endif // PEBBLE_H
