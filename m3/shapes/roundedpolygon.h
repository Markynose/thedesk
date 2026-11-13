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
 * Sources: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/RoundedPolygon.kt
 *          androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Shapes.kt
 *
 * Kotlin's overloaded RoundedPolygon(...) constructors are the static
 * fromNumVertices / fromVertices / fromFeatures factories here, and the
 * RoundedPolygon.Companion extension shapes (circle, rectangle, star, pill,
 * pillStar) are static members. A null perVertexRounding list is an empty
 * vector.
 */
#ifndef M3SHAPES_ROUNDEDPOLYGON_H
#define M3SHAPES_ROUNDEDPOLYGON_H

#include "cornerrounding.h"
#include "cubic.h"
#include "feature.h"
#include "utils.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace m3shapes {

    /**
     * The RoundedPolygon class allows simple construction of polygonal shapes with optional rounding
     * at the vertices. Polygons can be constructed with either the number of vertices desired or an
     * ordered list of vertices.
     */
    class RoundedPolygon {
        public:
            RoundedPolygon(std::vector<Feature> features, Point center);

            /**
             * This constructor takes the number of vertices in the resulting polygon. These vertices
             * are positioned on a virtual circle around a given center with each vertex positioned
             * [radius] distance from that center, equally spaced (with equal angles between them). If
             * no radius is supplied, the shape will be created with a default radius of 1, resulting
             * in a shape whose vertices lie on a unit circle, with width/height of 2. That default
             * polygon will probably need to be rescaled using [transformed] into the appropriate size
             * for the UI in which it will be drawn.
             */
            static RoundedPolygon fromNumVertices(int numVertices, float radius = 1.f, float centerX = 0.f,
                float centerY = 0.f, CornerRounding rounding = Unrounded,
                const std::vector<CornerRounding>& perVertexRounding = {});

            /**
             * This function takes the vertices (either supplied or calculated, depending on the
             * constructor called), plus [CornerRounding] parameters, and creates the actual
             * [RoundedPolygon] shape, rounding around the vertices (or not) as specified. The result
             * is a list of [Cubic] curves which represent the geometry of the final shape.
             *
             * @param vertices The list of vertices in this polygon specified as pairs of x/y
             *   coordinates in this FloatArray. This should be an ordered list (with the outline of the
             *   shape going from each vertex to the next in order of this list), otherwise the results
             *   will be undefined.
             * @param centerX/centerY The center of this polygon; if left at Kotlin's
             *   Float.MIN_VALUE it is calculated from the vertices.
             */
            static RoundedPolygon fromVertices(const std::vector<float>& vertices, CornerRounding rounding = Unrounded,
                const std::vector<CornerRounding>& perVertexRounding = {},
                float centerX = KotlinFloatMinValue, float centerY = KotlinFloatMinValue);

            /**
             * Creates a polygon from the given [Feature]s. A NaN center is calculated as the
             * average of the anchor points.
             */
            static RoundedPolygon fromFeatures(std::vector<Feature> features,
                float centerX = std::numeric_limits<float>::quiet_NaN(),
                float centerY = std::numeric_limits<float>::quiet_NaN());

            // ---- Shapes.kt ----

            // A circle approximated by [numVertices] rounded vertices around ([centerX], [centerY]).
            static RoundedPolygon circle(int numVertices = 8, float radius = 1.f, float centerX = 0.f, float centerY = 0.f);

            // A rectangle with the given [width] and [height], centered at ([centerX], [centerY]).
            static RoundedPolygon rectangle(float width = 2.f, float height = 2.f, CornerRounding rounding = Unrounded,
                const std::vector<CornerRounding>& perVertexRounding = {}, float centerX = 0.f, float centerY = 0.f);

            // A star with [numVerticesPerRadius] outer and inner vertices.
            // A null [innerRounding] (Kotlin: null) means inner vertices use [rounding] too.
            static RoundedPolygon star(int numVerticesPerRadius, float radius = 1.f, float innerRadius = .5f,
                CornerRounding rounding = Unrounded, const CornerRounding* innerRounding = nullptr,
                const std::vector<CornerRounding>& perVertexRounding = {}, float centerX = 0.f, float centerY = 0.f);

            // A pill shape consisting of a rectangle shape bounded by two semicircles at either of
            // the long ends of the rectangle.
            static RoundedPolygon pill(float width = 2.f, float height = 1.f, float smoothing = 0.f,
                float centerX = 0.f, float centerY = 0.f);

            // A pillStar shape: like a pill() but with inner and outer vertices around its outline.
            static RoundedPolygon pillStar(float width = 2.f, float height = 1.f, int numVerticesPerRadius = 8,
                float innerRadiusRatio = .5f, CornerRounding rounding = Unrounded,
                const CornerRounding* innerRounding = nullptr,
                const std::vector<CornerRounding>& perVertexRounding = {}, float vertexSpacing = 0.5f,
                float startLocation = 0.f, float centerX = 0.f, float centerY = 0.f);

            // ---- RoundedPolygon ----

            const std::vector<Feature>& features() const { return m_features; }
            Point center() const { return m_center; }
            float centerX() const { return m_center.x; }
            float centerY() const { return m_center.y; }

            // A flattened version of the [Feature]s, as a List<Cubic>.
            const std::vector<Cubic>& cubics() const { return m_cubics; }

            // Transforms a [RoundedPolygon] with the given [PointTransformer]
            RoundedPolygon transformed(const PointTransformer& f) const;

            // Creates a new RoundedPolygon, moving and resizing this one, so it's completely inside
            // the (0, 0) -> (1, 1) square, centered if there extra space in one direction
            RoundedPolygon normalized() const;

            // Like [calculateBounds], this function calculates the axis-aligned bounds of the
            // object and returns that rectangle. But this function determines the max dimension of
            // the shape (by calculating the distance from its center to the start and midpoint of
            // each curve) and returns a square which can be used to hold the object in any rotation.
            std::array<float, 4> calculateMaxBounds() const;

            // Calculates the axis-aligned bounds of the object (left, top, right, bottom).
            // @param approximate when true, uses a faster calculation to create the bounding box
            //   based on the min/max values of all anchor and control points that make up the shape.
            std::array<float, 4> calculateBounds(bool approximate = true) const;

        private:
            std::vector<Feature> m_features;
            Point m_center;
            std::vector<Cubic> m_cubics;
    };

    Point calculateCenter(const std::vector<float>& vertices);

} // namespace m3shapes

#endif // M3SHAPES_ROUNDEDPOLYGON_H
