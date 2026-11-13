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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Cubic.kt
 */
#ifndef M3SHAPES_CUBIC_H
#define M3SHAPES_CUBIC_H

#include "point.h"

#include <array>
#include <utility>

namespace m3shapes {

    /**
     * This class holds the anchor and control point data for a single cubic Bézier curve, with
     * anchor points ([anchor0X], [anchor0Y]) and ([anchor1X], [anchor1Y]) at either end and control
     * points ([control0X], [control0Y]) and ([control1X], [control1Y]) determining the slope of the
     * curve between the anchor points.
     */
    class Cubic {
        public:
            // anchor0X, anchor0Y, control0X, control0Y, control1X, control1Y, anchor1X, anchor1Y
            std::array<float, 8> points{};

            Cubic() = default;
            explicit Cubic(const std::array<float, 8>& points) :
                points(points) {}
            Cubic(float anchor0X, float anchor0Y, float control0X, float control0Y,
                float control1X, float control1Y, float anchor1X, float anchor1Y) :
                points{anchor0X, anchor0Y, control0X, control0Y, control1X, control1Y, anchor1X, anchor1Y} {}
            Cubic(const Point& anchor0, const Point& control0, const Point& control1, const Point& anchor1) :
                points{anchor0.x, anchor0.y, control0.x, control0.y, control1.x, control1.y, anchor1.x, anchor1.y} {}

            float anchor0X() const { return points[0]; }
            float anchor0Y() const { return points[1]; }
            float control0X() const { return points[2]; }
            float control0Y() const { return points[3]; }
            float control1X() const { return points[4]; }
            float control1Y() const { return points[5]; }
            float anchor1X() const { return points[6]; }
            float anchor1Y() const { return points[7]; }

            // Returns a point on the curve for parameter t, representing the proportional distance
            // along the curve between its starting point at anchor0 and ending point at anchor1.
            Point pointOnCurve(float t) const;

            bool zeroLength() const;

            bool convexTo(const Cubic& next) const;

            // Finds the bounds of this cubic (left, top, right, bottom). With [approximate] the
            // bounds of all anchor and control points are used, which is faster but may be larger.
            void calculateBounds(std::array<float, 4>& bounds, bool approximate = false) const;

            // Returns two Cubics, created by splitting this curve at the given distance of [t]
            // between the original starting and ending anchor points.
            std::pair<Cubic, Cubic> split(float t) const;

            // Utility function to reverse the control/anchor points for this curve.
            Cubic reverse() const;

            Cubic operator+(const Cubic& o) const;
            Cubic operator*(float x) const;
            Cubic operator/(float x) const { return *this * (1.f / x); }
            bool operator==(const Cubic& other) const { return points == other.points; }

            // Transforms the points in this [Cubic] with the given [PointTransformer] and returns
            // a new [Cubic]
            Cubic transformed(const PointTransformer& f) const;

            // Generates a bezier curve that is a straight line between the given anchor points.
            static Cubic straightLine(float x0, float y0, float x1, float y1);

            // Generates a bezier curve that approximates a circular arc, with p0 and p1 as the
            // starting and ending anchor points. The curve generated is the smallest of the two
            // possible arcs around the entire 360-degree circle. Arcs of greater than 180 degrees
            // should use more than one arc together.
            static Cubic circularArc(float centerX, float centerY, float x0, float y0, float x1, float y1);

            // Generates an empty Cubic defined at (x0, y0)
            static Cubic empty(float x0, float y0);

        private:
            static bool zeroIsh(float value);
    };

    // This is a Mutable version of [Cubic], used mostly for performance critical paths so we can
    // avoid creating new [Cubic]s.
    class MutableCubic : public Cubic {
        public:
            void transform(const PointTransformer& f);
            void interpolate(const Cubic& c1, const Cubic& c2, float progress);

        private:
            void transformOnePoint(const PointTransformer& f, int ix);
    };

} // namespace m3shapes

#endif // M3SHAPES_CUBIC_H
