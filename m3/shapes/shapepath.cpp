/*
 * Copyright 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Ported to C++/Qt for theDesk; modified.
 * Sources: androidx compose/material3/material3/src/commonMain/kotlin/
 *            androidx/compose/material3/internal/ShapeUtil.kt
 *            androidx/compose/material3/MaterialShapes.kt
 */
#include "shapepath.h"

#include <cmath>

namespace m3shapes {

    namespace {
        float radiansToDegrees(float radians) {
            return static_cast<float>(radians * 180.0 / 3.14159265358979323846);
        }
    } // namespace

    RoundedPolygon transformed(const RoundedPolygon& polygon, const QTransform& matrix) {
        return polygon.transformed([&matrix](float x, float y) {
            QPointF p = matrix.map(QPointF(x, y));
            return TransformResult(static_cast<float>(p.x()), static_cast<float>(p.y()));
        });
    }

    QPainterPath pathFromCubics(const std::vector<Cubic>& cubics, int startAngle, bool repeatPath, bool closePath,
        float rotationPivotX, float rotationPivotY) {
        QPainterPath path;
        bool first = true;
        bool haveFirstCubic = false;
        for (const Cubic& it : cubics) {
            if (first) {
                path.moveTo(it.anchor0X(), it.anchor0Y());
                if (startAngle != 0) haveFirstCubic = true;
                first = false;
            }
            path.cubicTo(it.control0X(), it.control0Y(), it.control1X(), it.control1Y(), it.anchor1X(), it.anchor1Y());
        }
        if (repeatPath) {
            bool firstInRepeat = true;
            for (const Cubic& it : cubics) {
                if (firstInRepeat) {
                    path.lineTo(it.anchor0X(), it.anchor0Y());
                    firstInRepeat = false;
                }
                path.cubicTo(it.control0X(), it.control0Y(), it.control1X(), it.control1Y(), it.anchor1X(), it.anchor1Y());
            }
        }

        if (closePath) path.closeSubpath();

        if (startAngle != 0 && haveFirstCubic) {
            float angleToFirstCubic = radiansToDegrees(
                kmath::atan2(cubics[0].anchor0Y() - rotationPivotY, cubics[0].anchor0X() - rotationPivotX));
            // Rotate the Path to to start from the given angle (about the origin, as upstream).
            QTransform rotation;
            rotation.rotate(-angleToFirstCubic + startAngle);
            path = rotation.map(path);
        }
        return path;
    }

    QPainterPath toPath(const RoundedPolygon& polygon, int startAngle, bool repeatPath, bool closePath) {
        return pathFromCubics(polygon.cubics(), startAngle, repeatPath, closePath, polygon.centerX(), polygon.centerY());
    }

    QPainterPath toPath(const Morph& morph, float progress, int startAngle, bool repeatPath, bool closePath,
        float rotationPivotX, float rotationPivotY) {
        return pathFromCubics(morph.asCubics(progress), startAngle, repeatPath, closePath, rotationPivotX, rotationPivotY);
    }

    QPainterPath fitToRect(const QPainterPath& normalizedPath, const QRectF& rect) {
        QPainterPath path = QTransform::fromScale(rect.width(), rect.height()).map(normalizedPath);
        // Scale and translate the path to align its center with the available size center.
        // (Skia/Compose Path.getBounds() is the control point bounds.)
        path.translate(rect.center() - path.controlPointRect().center());
        return path;
    }

    QPainterPath toPath(const RoundedPolygon& normalizedPolygon, const QRectF& rect, int startAngle) {
        return fitToRect(toPath(normalizedPolygon, startAngle), rect);
    }

    QPainterPath toPath(const Morph& morph, float progress, const QRectF& rect, int startAngle) {
        return fitToRect(toPath(morph, progress, startAngle), rect);
    }

} // namespace m3shapes
