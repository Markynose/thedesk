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
#include "cubic.h"

#include "utils.h"

#include <algorithm>

namespace m3shapes {

    namespace {
        inline bool inUnitRange(float t) { return t >= 0.f && t <= 1.f; }
    } // namespace

    Point Cubic::pointOnCurve(float t) const {
        float u = 1 - t;
        return Point(
            anchor0X() * (u * u * u) +
                control0X() * (3 * t * u * u) +
                control1X() * (3 * t * t * u) +
                anchor1X() * (t * t * t),
            anchor0Y() * (u * u * u) +
                control0Y() * (3 * t * u * u) +
                control1Y() * (3 * t * t * u) +
                anchor1Y() * (t * t * t));
    }

    bool Cubic::zeroLength() const {
        return kmath::abs(anchor0X() - anchor1X()) < DistanceEpsilon &&
               kmath::abs(anchor0Y() - anchor1Y()) < DistanceEpsilon;
    }

    bool Cubic::convexTo(const Cubic& next) const {
        Point prevVertex = Point(anchor0X(), anchor0Y());
        Point currVertex = Point(anchor1X(), anchor1Y());
        Point nextVertex = Point(next.anchor1X(), next.anchor1Y());
        return convex(prevVertex, currVertex, nextVertex);
    }

    bool Cubic::zeroIsh(float value) {
        return kmath::abs(value) < DistanceEpsilon;
    }

    void Cubic::calculateBounds(std::array<float, 4>& bounds, bool approximate) const {
        // A curve might be of zero-length, with both anchors co-lated.
        // Just return the point itself.
        if (zeroLength()) {
            bounds[0] = anchor0X();
            bounds[1] = anchor0Y();
            bounds[2] = anchor0X();
            bounds[3] = anchor0Y();
            return;
        }

        float minX = std::min(anchor0X(), anchor1X());
        float minY = std::min(anchor0Y(), anchor1Y());
        float maxX = std::max(anchor0X(), anchor1X());
        float maxY = std::max(anchor0Y(), anchor1Y());

        if (approximate) {
            // Approximate bounds use the bounding box of all anchors and controls
            bounds[0] = std::min(minX, std::min(control0X(), control1X()));
            bounds[1] = std::min(minY, std::min(control0Y(), control1Y()));
            bounds[2] = std::max(maxX, std::max(control0X(), control1X()));
            bounds[3] = std::max(maxY, std::max(control0Y(), control1Y()));
            return;
        }

        // Find the derivative, which is a quadratic Bezier. Then we can solve for t using
        // the quadratic formula
        float xa = -anchor0X() + 3 * control0X() - 3 * control1X() + anchor1X();
        float xb = 2 * anchor0X() - 4 * control0X() + 2 * control1X();
        float xc = -anchor0X() + control0X();

        auto updateX = [&](float t) {
            if (inUnitRange(t)) {
                float it = pointOnCurve(t).x;
                if (it < minX) minX = it;
                if (it > maxX) maxX = it;
            }
        };
        auto updateY = [&](float t) {
            if (inUnitRange(t)) {
                float it = pointOnCurve(t).y;
                if (it < minY) minY = it;
                if (it > maxY) maxY = it;
            }
        };

        if (zeroIsh(xa)) {
            // Try Muller's method instead; it can find a single root when a is 0
            if (xb != 0.f) {
                float t = 2 * xc / (-2 * xb);
                updateX(t);
            }
        } else {
            float xs = xb * xb - 4 * xa * xc;
            if (xs >= 0) {
                float t1 = (-xb + kmath::sqrt(xs)) / (2 * xa);
                updateX(t1);

                float t2 = (-xb - kmath::sqrt(xs)) / (2 * xa);
                updateX(t2);
            }
        }

        // Repeat the above for y coordinate
        float ya = -anchor0Y() + 3 * control0Y() - 3 * control1Y() + anchor1Y();
        float yb = 2 * anchor0Y() - 4 * control0Y() + 2 * control1Y();
        float yc = -anchor0Y() + control0Y();

        if (zeroIsh(ya)) {
            if (yb != 0.f) {
                float t = 2 * yc / (-2 * yb);
                updateY(t);
            }
        } else {
            float ys = yb * yb - 4 * ya * yc;
            if (ys >= 0) {
                float t1 = (-yb + kmath::sqrt(ys)) / (2 * ya);
                updateY(t1);

                float t2 = (-yb - kmath::sqrt(ys)) / (2 * ya);
                updateY(t2);
            }
        }
        bounds[0] = minX;
        bounds[1] = minY;
        bounds[2] = maxX;
        bounds[3] = maxY;
    }

    std::pair<Cubic, Cubic> Cubic::split(float t) const {
        float u = 1 - t;
        Point pointOnCurve = this->pointOnCurve(t);
        return {
            Cubic(
                anchor0X(),
                anchor0Y(),
                anchor0X() * u + control0X() * t,
                anchor0Y() * u + control0Y() * t,
                anchor0X() * (u * u) + control0X() * (2 * u * t) + control1X() * (t * t),
                anchor0Y() * (u * u) + control0Y() * (2 * u * t) + control1Y() * (t * t),
                pointOnCurve.x,
                pointOnCurve.y),
            Cubic(
                // TODO: should calculate once and share the result
                pointOnCurve.x,
                pointOnCurve.y,
                control0X() * (u * u) + control1X() * (2 * u * t) + anchor1X() * (t * t),
                control0Y() * (u * u) + control1Y() * (2 * u * t) + anchor1Y() * (t * t),
                control1X() * u + anchor1X() * t,
                control1Y() * u + anchor1Y() * t,
                anchor1X(),
                anchor1Y())};
    }

    Cubic Cubic::reverse() const {
        return Cubic(anchor1X(), anchor1Y(), control1X(), control1Y(), control0X(), control0Y(), anchor0X(), anchor0Y());
    }

    Cubic Cubic::operator+(const Cubic& o) const {
        std::array<float, 8> r;
        for (int i = 0; i < 8; i++) r[i] = points[i] + o.points[i];
        return Cubic(r);
    }

    Cubic Cubic::operator*(float x) const {
        std::array<float, 8> r;
        for (int i = 0; i < 8; i++) r[i] = points[i] * x;
        return Cubic(r);
    }

    Cubic Cubic::transformed(const PointTransformer& f) const {
        MutableCubic newCubic;
        newCubic.points = points;
        newCubic.transform(f);
        return Cubic(newCubic.points);
    }

    Cubic Cubic::straightLine(float x0, float y0, float x1, float y1) {
        return Cubic(
            x0,
            y0,
            m3shapes::interpolate(x0, x1, 1.f / 3.f),
            m3shapes::interpolate(y0, y1, 1.f / 3.f),
            m3shapes::interpolate(x0, x1, 2.f / 3.f),
            m3shapes::interpolate(y0, y1, 2.f / 3.f),
            x1,
            y1);
    }

    // TODO: consider a more general function (maybe in addition to this) that allows
    // caller to get a list of curves surpassing 180 degrees
    Cubic Cubic::circularArc(float centerX, float centerY, float x0, float y0, float x1, float y1) {
        Point p0d = directionVector(x0 - centerX, y0 - centerY);
        Point p1d = directionVector(x1 - centerX, y1 - centerY);
        Point rotatedP0 = rotate90(p0d);
        Point rotatedP1 = rotate90(p1d);
        bool clockwise = rotatedP0.dotProduct(x1 - centerX, y1 - centerY) >= 0;
        float cosa = p0d.dotProduct(p1d);
        if (cosa > 0.999f) /* p0 ~= p1 */ return straightLine(x0, y0, x1, y1);
        float k = distance(x0 - centerX, y0 - centerY) * 4.f / 3.f *
                  (kmath::sqrt(2 * (1 - cosa)) - kmath::sqrt(1 - cosa * cosa)) / (1 - cosa) *
                  (clockwise ? 1.f : -1.f);
        return Cubic(
            x0,
            y0,
            x0 + rotatedP0.x * k,
            y0 + rotatedP0.y * k,
            x1 - rotatedP1.x * k,
            y1 - rotatedP1.y * k,
            x1,
            y1);
    }

    Cubic Cubic::empty(float x0, float y0) {
        return Cubic(x0, y0, x0, y0, x0, y0, x0, y0);
    }

    void MutableCubic::transformOnePoint(const PointTransformer& f, int ix) {
        TransformResult result = f(points[ix], points[ix + 1]);
        points[ix] = result.x;
        points[ix + 1] = result.y;
    }

    void MutableCubic::transform(const PointTransformer& f) {
        transformOnePoint(f, 0);
        transformOnePoint(f, 2);
        transformOnePoint(f, 4);
        transformOnePoint(f, 6);
    }

    void MutableCubic::interpolate(const Cubic& c1, const Cubic& c2, float progress) {
        for (int i = 0; i < 8; i++) points[i] = m3shapes::interpolate(c1.points[i], c2.points[i], progress);
    }

} // namespace m3shapes
