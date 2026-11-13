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
 *            androidx/compose/material3/internal/ShapeUtil.kt   (toPath, transformed)
 *            androidx/compose/material3/MaterialShapes.kt       (toShape's createOutline)
 *          The Compose Path/Matrix types are QPainterPath/QTransform here.
 */
#ifndef M3SHAPES_SHAPEPATH_H
#define M3SHAPES_SHAPEPATH_H

#include "morph.h"
#include "roundedpolygon.h"

#include <QPainterPath>
#include <QRectF>
#include <QTransform>

namespace m3shapes {

    // Transforms a [RoundedPolygon] with the given matrix
    RoundedPolygon transformed(const RoundedPolygon& polygon, const QTransform& matrix);

    // Builds a path from cubics: moveTo the first anchor, then one cubicTo per cubic.
    // @param startAngle an angle (in degrees) to rotate the path to start drawing from, measured
    //   from the pivot; 0 leaves the path as is
    // @param repeatPath whether or not to repeat the path twice before closing it
    // @param closePath whether or not to close the created path
    QPainterPath pathFromCubics(const std::vector<Cubic>& cubics, int startAngle = 0, bool repeatPath = false,
        bool closePath = true, float rotationPivotX = 0.f, float rotationPivotY = 0.f);

    // Gets a QPainterPath for a [RoundedPolygon], in the polygon's own coordinates (0..1 for
    // normalised shapes). The default startAngle of 0 matches the public Compose
    // RoundedPolygon.toPath(); the rotation pivot is the polygon's center.
    QPainterPath toPath(const RoundedPolygon& polygon, int startAngle = 0, bool repeatPath = false,
        bool closePath = true);

    // Gets a QPainterPath for a [Morph] at [progress]. Defaults match the public Compose
    // Morph.toPath(progress, path, startAngle = 0).
    QPainterPath toPath(const Morph& morph, float progress, int startAngle = 0, bool repeatPath = false,
        bool closePath = true, float rotationPivotX = 0.f, float rotationPivotY = 0.f);

    // Compose RoundedPolygon.toShape().createOutline(): scales a path made from a *normalised*
    // shape by the rect's width and height, then centers its (control point) bounds in the rect.
    QPainterPath fitToRect(const QPainterPath& normalizedPath, const QRectF& rect);

    // Convenience: toPath() then fitToRect(); this is how Compose draws a MaterialShapes shape
    // as a Shape (e.g. Modifier.clip(MaterialShapes.Cookie9Sided.toShape())).
    QPainterPath toPath(const RoundedPolygon& normalizedPolygon, const QRectF& rect, int startAngle = 0);
    QPainterPath toPath(const Morph& morph, float progress, const QRectF& rect, int startAngle = 0);

} // namespace m3shapes

#endif // M3SHAPES_SHAPEPATH_H
