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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Utils.kt
 */
#ifndef M3SHAPES_UTILS_H
#define M3SHAPES_UTILS_H

#include "point.h"

#include <functional>
#include <limits>

namespace m3shapes {

    inline float distance(float x, float y) { return kmath::sqrt(x * x + y * y); }
    inline float distanceSquared(float x, float y) { return x * x + y * y; }

    // Returns unit vector representing the direction to this point from (0, 0)
    inline Point directionVector(float x, float y) {
        float d = distance(x, y);
        if (!(d > 0.f)) throw std::invalid_argument("Required distance greater than zero");
        return Point(x / d, y / d);
    }

    inline Point directionVector(float angleRadians) {
        return Point(kmath::cos(angleRadians), kmath::sin(angleRadians));
    }

    inline Point radialToCartesian(float radius, float angleRadians, Point center = Point(0, 0)) {
        return directionVector(angleRadians) * radius + center;
    }

    // Epsilon used throughout the library for the cut-off of "zero-ish" distances
    // (reasonable roundoff error, not larger than a pixel on any reasonable display).
    inline constexpr float DistanceEpsilon = 1e-4f;
    inline constexpr float AngleEpsilon = 1e-6f;
    // Relaxed epsilon for operations that allow higher tolerances.
    inline constexpr float RelaxedDistanceEpsilon = 5e-3f;

    inline Point rotate90(const Point& p) { return Point(-p.y, p.x); }

    inline constexpr Point Zero = Point(0.f, 0.f);

    // PI.toFloat()
    inline constexpr float FloatPi = 3.14159265358979323846f;
    // 2 * PI.toFloat()
    inline constexpr float TwoPi = 2 * FloatPi;

    // Kotlin's Float.MIN_VALUE is the smallest *positive* float, not the lowest one.
    inline constexpr float KotlinFloatMinValue = std::numeric_limits<float>::denorm_min();
    inline constexpr float KotlinFloatMaxValue = std::numeric_limits<float>::max();

    inline float square(float x) { return x * x; }

    // Similar to num % mod, but ensures the result is always positive.
    // For example: 4 % 3 = positiveModulo(4, 3) = 1, but: -4 % 3 = -1 positiveModulo(-4, 3) = 2
    inline float positiveModulo(float num, float mod) {
        return kmath::rem(kmath::rem(num, mod) + mod, mod);
    }

    // Returns whether C is on the line defined by the two points AB
    bool collinearIsh(float aX, float aY, float bX, float bY, float cX, float cY, float tolerance = DistanceEpsilon);

    // Approximates whether corner at this vertex is concave or convex, based on the relationship
    // of the prev->curr/curr->next vectors.
    bool convex(const Point& previous, const Point& current, const Point& next);

    // Does a ternary search in [v0..v1] to find the parameter that minimizes the given function.
    // Stops when the search space size is reduced below the given tolerance.
    using FindMinimumFunction = std::function<float(float)>;
    float findMinimum(float v0, float v1, float tolerance, const FindMinimumFunction& f);

} // namespace m3shapes

#endif // M3SHAPES_UTILS_H
