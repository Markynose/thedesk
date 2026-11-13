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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Point.kt
 */
#ifndef M3SHAPES_POINT_H
#define M3SHAPES_POINT_H

#include <cmath>
#include <functional>
#include <stdexcept>

namespace m3shapes {

    // Kotlin's Float maths on the JVM goes through double and rounds back to
    // float. These helpers do the same so results match Android bit for bit
    // wherever that matters.
    namespace kmath {
        inline float sqrt(float x) { return static_cast<float>(std::sqrt(static_cast<double>(x))); }
        inline float cos(float x) { return static_cast<float>(std::cos(static_cast<double>(x))); }
        inline float sin(float x) { return static_cast<float>(std::sin(static_cast<double>(x))); }
        inline float atan2(float y, float x) { return static_cast<float>(std::atan2(static_cast<double>(y), static_cast<double>(x))); }
        inline float abs(float x) { return std::fabs(x); }
        // Kotlin's Float `%` is IEEE fmod (truncated, sign of the dividend)
        inline float rem(float a, float b) { return std::fmod(a, b); }
    } // namespace kmath

    // Point.kt: `typealias Point = FloatFloatPair`
    struct Point {
            float x = 0;
            float y = 0;

            constexpr Point() = default;
            constexpr Point(float x, float y) :
                x(x), y(y) {}

            Point copy(float nx, float ny) const { return Point(nx, ny); }

            float getDistance() const { return kmath::sqrt(x * x + y * y); }
            float getDistanceSquared() const { return x * x + y * y; }
            float dotProduct(const Point& other) const { return x * other.x + y * other.y; }
            float dotProduct(float otherX, float otherY) const { return x * otherX + y * otherY; }

            // Compute the Z coordinate of the cross product of two vectors, to check if the second
            // vector is going clockwise ( > 0 ) or counterclockwise (< 0) compared with the first
            // one. It could also be 0, if the vectors are co-linear.
            bool clockwise(const Point& other) const { return x * other.y - y * other.x > 0; }

            // Returns unit vector representing the direction to this point from (0, 0)
            Point getDirection() const {
                float d = getDistance();
                if (!(d > 0.f)) throw std::invalid_argument("Can't get the direction of a 0-length vector");
                return *this / d;
            }

            Point operator-() const { return Point(-x, -y); }
            Point operator-(const Point& o) const { return Point(x - o.x, y - o.y); }
            Point operator+(const Point& o) const { return Point(x + o.x, y + o.y); }
            Point operator*(float operand) const { return Point(x * operand, y * operand); }
            Point operator/(float operand) const { return Point(x / operand, y / operand); }
            Point operator%(float operand) const { return Point(kmath::rem(x, operand), kmath::rem(y, operand)); }
            bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    };

    // Cubic.kt: `fun interface PointTransformer { fun transform(x, y): TransformResult }`
    using TransformResult = Point;
    using PointTransformer = std::function<TransformResult(float x, float y)>;

    inline float interpolate(float start, float stop, float fraction) {
        return (1 - fraction) * start + fraction * stop;
    }

    inline Point interpolate(const Point& start, const Point& stop, float fraction) {
        return Point(interpolate(start.x, stop.x, fraction), interpolate(start.y, stop.y, fraction));
    }

    inline Point transformed(const Point& p, const PointTransformer& f) {
        return f(p.x, p.y);
    }

} // namespace m3shapes

#endif // M3SHAPES_POINT_H
