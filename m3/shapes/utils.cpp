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
#include "utils.h"

namespace m3shapes {

    bool collinearIsh(float aX, float aY, float bX, float bY, float cX, float cY, float tolerance) {
        // The dot product of a perpendicular angle is 0. By rotating one of the vectors,
        // we save the calculations to convert the dot product to degrees afterwards.
        Point ab = rotate90(Point(bX - aX, bY - aY));
        Point ac = Point(cX - aX, cY - aY);
        float dotProduct = kmath::abs(ab.dotProduct(ac));
        float relativeTolerance = tolerance * ab.getDistance() * ac.getDistance();

        return dotProduct < tolerance || dotProduct < relativeTolerance;
    }

    bool convex(const Point& previous, const Point& current, const Point& next) {
        // TODO: b/369320447 - This is a fast, but not reliable calculation.
        return (current - previous).clockwise(next - current);
    }

    float findMinimum(float v0, float v1, float tolerance, const FindMinimumFunction& f) {
        float a = v0;
        float b = v1;
        while (b - a > tolerance) {
            float c1 = (2 * a + b) / 3;
            float c2 = (2 * b + a) / 3;
            if (f(c1) < f(c2)) {
                b = c2;
            } else {
                a = c1;
            }
        }
        return (a + b) / 2;
    }

} // namespace m3shapes
