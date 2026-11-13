/*
 * Copyright 2022 The Android Open Source Project
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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/CornerRounding.kt
 */
#ifndef M3SHAPES_CORNERROUNDING_H
#define M3SHAPES_CORNERROUNDING_H

namespace m3shapes {

    /**
     * CornerRounding defines the amount and quality around a given vertex of a shape. [radius]
     * defines the radius of the circle which forms the basis of the rounding for the vertex.
     * [smoothing] defines the amount by which the curve is extended from the circular arc around the
     * corner to the edge between vertices.
     *
     * Each corner of a shape can be thought of as either: <em>
     * <li> unrounded (with a corner radius of 0 and no smoothing) </li>
     * <li> rounded with only a circular arc (with smoothing of 0). In this case, the rounding around
     *   the corner follows an approximated circular arc between the edges to adjacent vertices.
     * </li>
     * <li> rounded with three curves: There is an inner circular arc and two symmetric flanking
     *   curves. The flanking curves determine the curvature from the inner curve to the edges, with
     *   a value of 0 (no smoothing) meaning that it is purely a circular curve and a value of 1
     *   meaning that the flanking curves are maximized between the inner curve and the edges.
     * </li></em>
     */
    struct CornerRounding {
            float radius = 0;    // >= 0
            float smoothing = 0; // 0..1

            constexpr CornerRounding() = default;
            constexpr CornerRounding(float radius, float smoothing = 0) :
                radius(radius), smoothing(smoothing) {}

            bool operator==(const CornerRounding&) const = default;
    };

    inline constexpr CornerRounding Unrounded = CornerRounding();

} // namespace m3shapes

#endif // M3SHAPES_CORNERROUNDING_H
