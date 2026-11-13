/*
 * Copyright 2023 The Android Open Source Project
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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Features.kt
 *
 * The Kotlin abstract class with Edge/Corner subclasses is a single value
 * type here with a kind tag; the behaviour is the same.
 */
#ifndef M3SHAPES_FEATURE_H
#define M3SHAPES_FEATURE_H

#include "cubic.h"

#include <vector>

namespace m3shapes {

    /**
     * While a polygon's shape can be drawn solely using a list of [Cubic] objects representing its
     * raw curves and lines, features add an extra layer of context to groups of cubics. Features
     * group cubics into (straight) edges, convex corners, or concave corners. For example, rounding a
     * rectangle adds many cubics around its edges, but the rectangle's overall number of corners
     * remains the same. [Morph] therefore uses this grouping for several reasons:
     * - Noise Reduction: Grouping cubics reduces the amount of noise introduced by individual cubics
     *   (as seen in the rounded rectangle example).
     * - Mapping Base: The grouping serves as the base set for [Morph]'s mapping process.
     * - Curve Type Mapping: [Morph] maps similar curve types (convex, concave) together. Note that
     *   edges or features created with [buildIgnorableFeature] are ignored in the default mapping.
     */
    class Feature {
        public:
            enum class Kind {
                Edge,
                Corner
            };

            // Factory
            // Group a list of [Cubic] objects to a feature that should be ignored in the default
            // [Morph] mapping. The feature can have any indentation.
            static Feature buildIgnorableFeature(const std::vector<Cubic>& cubics);
            // Group a [Cubic] object to an edge (neither inward or outward identification in a shape).
            static Feature buildEdge(const Cubic& cubic);
            // Group a list of [Cubic] objects to a convex corner (outward indentation in a shape).
            static Feature buildConvexCorner(const std::vector<Cubic>& cubics);
            // Group a list of [Cubic] objects to a concave corner (inward indentation in a shape).
            static Feature buildConcaveCorner(const std::vector<Cubic>& cubics);

            // Internal constructors (Feature.Edge(...) / Feature.Corner(...)), unvalidated
            static Feature edge(std::vector<Cubic> cubics);
            static Feature corner(std::vector<Cubic> cubics, bool convex = true);

            const std::vector<Cubic>& cubics() const { return m_cubics; }
            Kind kind() const { return m_kind; }

            // Transforms the points in this [Feature] with the given [PointTransformer] and returns
            // a new [Feature]
            Feature transformed(const PointTransformer& f) const;

            // Returns a new [Feature] with the points that define the shape of this [Feature] in
            // reversed order.
            Feature reversed() const;

            bool isIgnorableFeature() const { return m_kind == Kind::Edge; }
            bool isEdge() const { return m_kind == Kind::Edge; }
            bool isCorner() const { return m_kind == Kind::Corner; }
            bool isConvexCorner() const { return m_kind == Kind::Corner && m_convex; }
            bool isConcaveCorner() const { return m_kind == Kind::Corner && !m_convex; }
            // Corner.convex
            bool convex() const { return m_convex; }

        private:
            Feature(Kind kind, std::vector<Cubic> cubics, bool convex);
            static Feature validated(Feature feature);
            static bool isContinuous(const Feature& feature);

            Kind m_kind;
            std::vector<Cubic> m_cubics;
            bool m_convex;
    };

} // namespace m3shapes

#endif // M3SHAPES_FEATURE_H
