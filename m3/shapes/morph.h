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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Morph.kt
 */
#ifndef M3SHAPES_MORPH_H
#define M3SHAPES_MORPH_H

#include "cubic.h"
#include "roundedpolygon.h"

#include <array>
#include <utility>
#include <vector>

namespace m3shapes {

    /**
     * This class is used to animate between start and end polygons objects.
     *
     * Morphing between arbitrary objects can be problematic because it can be difficult to
     * determine how the points of a given shape map to the points of some other shape. [Morph]
     * simplifies the problem by only operating on [RoundedPolygon] objects, which are known to have
     * similar, contiguous structures. For one thing, the shape of a polygon is contiguous from start
     * to end (compared to an arbitrary Path object, which could have one or more `moveTo` operations
     * in the shape). Also, all edges of a polygon shape are represented by [Cubic] objects, thus the
     * start and end shapes use similar operations. Two Polygon shapes then only differ in the
     * quantity and placement of their curves. The morph works by determining how to map the curves
     * of the two shapes together (based on proximity and other information, such as distance to
     * polygon vertices and concavity), and splitting curves when the shapes do not have the same
     * number of curves or when the curve placement within the shapes is very different.
     */
    class Morph {
        public:
            Morph(const RoundedPolygon& start, const RoundedPolygon& end);

            const RoundedPolygon& start() const { return m_start; }
            const RoundedPolygon& end() const { return m_end; }

            // The structure which holds the actual shape being morphed. It contains all cubics
            // necessary to represent the start and end shapes (the original cubics in the shapes
            // may be cut to align the start/end shapes), matched one to one in each Pair.
            const std::vector<std::pair<Cubic, Cubic>>& morphMatch() const { return m_morphMatch; }

            // Calculates the axis-aligned bounds of the object (left, top, right, bottom).
            std::array<float, 4> calculateBounds(bool approximate = true) const;

            // Like [calculateBounds], but returns a square which can hold the object in any
            // rotation (see RoundedPolygon::calculateMaxBounds).
            std::array<float, 4> calculateMaxBounds() const;

            /**
             * Returns a representation of the morph object at a given [progress] value as a list
             * of Cubics. Note that this function causes a new list to be created and populated, so
             * there is some overhead.
             *
             * @param progress a value from 0 to 1 that determines the morph's current shape,
             *   between the start and end shapes provided at construction time. A value of 0
             *   results in the start shape, a value of 1 results in the end shape, and any value in
             *   between results in a shape which is a linear interpolation between those two
             *   shapes. The range is generally [0..1] and values outside could result in
             *   undefined shapes, but values close to (but outside) the range can be used to get
             *   an exaggerated effect (e.g., for a bounce or overshoot animation).
             */
            std::vector<Cubic> asCubics(float progress) const;

            /**
             * Returns a representation of the morph object at a given [progress] value, iterating
             * over the cubics and calling the callback. This function is faster than [asCubics],
             * since it doesn't allocate new [Cubic] instances, but to do this it reuses the same
             * [MutableCubic] instance during iteration.
             */
            template <typename Callback>
            void forEachCubic(float progress, MutableCubic& mutableCubic, Callback&& callback) const {
                for (const auto& [first, second] : m_morphMatch) {
                    mutableCubic.interpolate(first, second, progress);
                    callback(mutableCubic);
                }
            }

            template <typename Callback>
            void forEachCubic(float progress, Callback&& callback) const {
                MutableCubic mutableCubic;
                forEachCubic(progress, mutableCubic, std::forward<Callback>(callback));
            }

            // Kotlin: internal companion fun match(p1, p2)
            static std::vector<std::pair<Cubic, Cubic>> match(const RoundedPolygon& p1, const RoundedPolygon& p2);

        private:
            RoundedPolygon m_start;
            RoundedPolygon m_end;
            std::vector<std::pair<Cubic, Cubic>> m_morphMatch;
    };

} // namespace m3shapes

#endif // M3SHAPES_MORPH_H
