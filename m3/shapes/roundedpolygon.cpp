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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/RoundedPolygon.kt
 */
#include "roundedpolygon.h"

#include <algorithm>
#include <optional>

namespace m3shapes {

    namespace {

        /**
         * Private utility class that holds the information about each corner in a polygon. The
         * shape of the corner can be returned by calling the [getCubics] function, which will
         * return a list of curves representing the corner geometry. The shape of the corner
         * depends on the [rounding] constructor parameter.
         *
         * If rounding is null, there is no rounding; the corner will simply be a single point at
         * [p1]. This point will be represented by a [Cubic] of length 0 at that point.
         *
         * If rounding is not null, the corner will be rounded either with a curve approximating a
         * circular arc of the radius specified in [rounding], or with three curves if [rounding]
         * has a nonzero smoothing parameter. These three curves are a circular arc in the middle
         * and two symmetrical flanking curves on either side. The smoothing parameter determines
         * the curvature of the flanking curves.
         */
        class RoundedCorner {
            public:
                Point p0;
                Point p1;
                Point p2;
                CornerRounding rounding;

                Point d1;
                Point d2;
                float cornerRadius;
                float smoothing;
                float cosAngle;
                float sinAngle;
                float expectedRoundCut;

                // the center of the circle approximated by the rounding curve (or the middle of the
                // three curves if smoothing is requested). The center is the same as p0 if there is
                // no rounding.
                Point center = Point(0.f, 0.f);

                RoundedCorner(Point p0, Point p1, Point p2, CornerRounding rounding) :
                    p0(p0), p1(p1), p2(p2), rounding(rounding) {
                    Point v01 = p0 - p1;
                    Point v21 = p2 - p1;
                    float d01 = v01.getDistance();
                    float d21 = v21.getDistance();
                    if (d01 > 0.f && d21 > 0.f) {
                        d1 = v01 / d01;
                        d2 = v21 / d21;
                        cornerRadius = rounding.radius;
                        smoothing = rounding.smoothing;
                        // cosine of angle at p1 is dot product of unit vectors to the other two vertices
                        cosAngle = d1.dotProduct(d2);
                        // identity: sin^2 + cos^2 = 1
                        // sinAngle gives us the intersection
                        sinAngle = kmath::sqrt(1 - square(cosAngle));
                        // How much we need to cut, as measured on a side, to get the required radius
                        // calculating where the rounding circle hits the edge
                        // This uses the identity of tan(A/2) = sinA/(1 + cosA), where tan(A/2) = radius/cut
                        expectedRoundCut = (sinAngle > 1e-3) ? cornerRadius * (cosAngle + 1) / sinAngle : 0.f;
                    } else {
                        // One (or both) of the sides is empty, not much we can do.
                        d1 = Point(0.f, 0.f);
                        d2 = Point(0.f, 0.f);
                        cornerRadius = 0.f;
                        smoothing = 0.f;
                        cosAngle = 0.f;
                        sinAngle = 0.f;
                        expectedRoundCut = 0.f;
                    }
                }

                // smoothing changes the actual cut. 0 is same as expectedRoundCut, 1 doubles it
                float expectedCut() const { return (1 + smoothing) * expectedRoundCut; }

                std::vector<Cubic> getCubics(float allowedCut0, float allowedCut1) {
                    // We use the minimum of both cuts to determine the radius, but if there is more
                    // space in one side we can use it for smoothing.
                    float allowedCut = std::min(allowedCut0, allowedCut1);
                    // Nothing to do, just use lines, or a point
                    if (expectedRoundCut < DistanceEpsilon ||
                        allowedCut < DistanceEpsilon ||
                        cornerRadius < DistanceEpsilon) {
                        center = p1;
                        return {Cubic::straightLine(p1.x, p1.y, p1.x, p1.y)};
                    }
                    // How much of the cut is required for the rounding part.
                    float actualRoundCut = std::min(allowedCut, expectedRoundCut);
                    // We have two smoothing values, one for each side of the vertex
                    // Space is used for rounding values first. If there is space left over, then we
                    // apply smoothing, if it was requested
                    float actualSmoothing0 = calculateActualSmoothingValue(allowedCut0);
                    float actualSmoothing1 = calculateActualSmoothingValue(allowedCut1);
                    // Scale the radius if needed
                    float actualR = cornerRadius * actualRoundCut / expectedRoundCut;
                    // Distance from the corner (p1) to the center
                    float centerDistance = kmath::sqrt(square(actualR) + square(actualRoundCut));
                    // Center of the arc we will use for rounding
                    center = p1 + ((d1 + d2) / 2.f).getDirection() * centerDistance;
                    Point circleIntersection0 = p1 + d1 * actualRoundCut;
                    Point circleIntersection2 = p1 + d2 * actualRoundCut;
                    Cubic flanking0 = computeFlankingCurve(actualRoundCut, actualSmoothing0, p1, p0,
                        circleIntersection0, circleIntersection2, center, actualR);
                    Cubic flanking2 = computeFlankingCurve(actualRoundCut, actualSmoothing1, p1, p2,
                        circleIntersection2, circleIntersection0, center, actualR)
                                          .reverse();
                    return {
                        flanking0,
                        Cubic::circularArc(center.x, center.y, flanking0.anchor1X(), flanking0.anchor1Y(),
                            flanking2.anchor0X(), flanking2.anchor0Y()),
                        flanking2,
                    };
                }

            private:
                // If allowedCut is between expectedRoundCut and expectedCut, smoothing is
                // interpolated between 0 and the requested amount.
                float calculateActualSmoothingValue(float allowedCut) const {
                    if (allowedCut > expectedCut()) {
                        return smoothing;
                    } else if (allowedCut > expectedRoundCut) {
                        return smoothing * (allowedCut - expectedRoundCut) / (expectedCut() - expectedRoundCut);
                    } else {
                        return 0.f;
                    }
                }

                // Compute a flanking curve for a corner: an extension from the edge into the
                // circular arc at the center of the corner rounding.
                Cubic computeFlankingCurve(float actualRoundCut, float actualSmoothingValues, Point corner,
                    Point sideStart, Point circleSegmentIntersection, Point otherCircleSegmentIntersection,
                    Point circleCenter, float actualR) const {
                    // sideStart is the anchor, 'anchor' is actual control point
                    Point sideDirection = (sideStart - corner).getDirection();
                    Point curveStart = corner + sideDirection * actualRoundCut * (1 + actualSmoothingValues);
                    // We use an approximation to cut a part of the circle section proportional to
                    // 1 - smooth, When smooth = 0, we take the full section, when smooth = 1, we
                    // take nothing.
                    // TODO: revisit this, it can be problematic as it approaches 180 degrees
                    Point p = interpolate(circleSegmentIntersection,
                        (circleSegmentIntersection + otherCircleSegmentIntersection) / 2.f, actualSmoothingValues);
                    // The flanking curve ends on the circle
                    Point curveEnd = circleCenter + directionVector(p.x - circleCenter.x, p.y - circleCenter.y) * actualR;
                    // The anchor on the circle segment side is in the intersection between the
                    // tangent to the circle in the circle/flanking curve boundary and the linear
                    // segment.
                    Point circleTangent = rotate90(curveEnd - circleCenter);
                    Point anchorEnd = lineIntersection(sideStart, sideDirection, curveEnd, circleTangent)
                                          .value_or(circleSegmentIntersection);
                    // From what remains, we pick a point for the start anchor.
                    // 2/3 seems to come from design tools?
                    Point anchorStart = (curveStart + anchorEnd * 2.f) / 3.f;
                    return Cubic(curveStart, anchorStart, anchorEnd, curveEnd);
                }

                // Returns the intersection point of the two lines d0->d1 and p0->p1, or null if the
                // lines do not intersect
                static std::optional<Point> lineIntersection(Point p0, Point d0, Point p1, Point d1) {
                    Point rotatedD1 = rotate90(d1);
                    float den = d0.dotProduct(rotatedD1);
                    if (kmath::abs(den) < DistanceEpsilon) return std::nullopt;
                    float num = (p1 - p0).dotProduct(rotatedD1);
                    // Also check the relative value. This is equivalent to abs(den/num) <
                    // DistanceEpsilon, but avoid doing a division
                    if (kmath::abs(den) < DistanceEpsilon * kmath::abs(num)) return std::nullopt;
                    float k = num / den;
                    return p0 + d0 * k;
                }
        };

        std::vector<float> verticesFromNumVerts(int numVertices, float radius, float centerX, float centerY) {
            std::vector<float> result(numVertices * 2);
            int arrayIndex = 0;
            for (int i = 0; i < numVertices; i++) {
                Point vertex = radialToCartesian(radius, (FloatPi / numVertices * 2 * i)) + Point(centerX, centerY);
                result[arrayIndex++] = vertex.x;
                result[arrayIndex++] = vertex.y;
            }
            return result;
        }

    } // namespace

    RoundedPolygon::RoundedPolygon(std::vector<Feature> features, Point center) :
        m_features(std::move(features)), m_center(center) {
        // The first/last mechanism here ensures that the final anchor point in the shape
        // exactly matches the first anchor point. There can be rendering artifacts introduced
        // by those points being slightly off, even by much less than a pixel
        std::optional<Cubic> firstCubic;
        std::optional<Cubic> lastCubic;
        std::optional<std::vector<Cubic>> firstFeatureSplitStart;
        std::optional<std::vector<Cubic>> firstFeatureSplitEnd;
        if (!m_features.empty() && m_features[0].cubics().size() == 3) {
            const Cubic& centerCubic = m_features[0].cubics()[1];
            auto [start, end] = centerCubic.split(.5f);
            firstFeatureSplitStart = std::vector<Cubic>{m_features[0].cubics()[0], start};
            firstFeatureSplitEnd = std::vector<Cubic>{end, m_features[0].cubics()[2]};
        }
        // iterating one past the features list size allows us to insert the initial split
        // cubic if it exists
        for (size_t i = 0; i <= m_features.size(); i++) {
            const std::vector<Cubic>* featureCubics;
            if (i == 0 && firstFeatureSplitEnd) {
                featureCubics = &*firstFeatureSplitEnd;
            } else if (i == m_features.size()) {
                if (firstFeatureSplitStart) featureCubics = &*firstFeatureSplitStart;
                else break;
            } else {
                featureCubics = &m_features[i].cubics();
            }

            for (const Cubic& cubic : *featureCubics) {
                // Skip zero-length curves; they add nothing and can trigger rendering artifacts
                if (!cubic.zeroLength()) {
                    if (lastCubic) m_cubics.push_back(*lastCubic);
                    lastCubic = cubic;
                    if (!firstCubic) firstCubic = cubic;
                } else {
                    if (lastCubic) {
                        // Dropping several zero-ish length curves in a row can lead to
                        // enough discontinuity to throw an exception later, even though the
                        // distances are quite small. Account for that by making the last
                        // cubic use the latest anchor point, always.
                        lastCubic->points[6] = cubic.anchor1X();
                        lastCubic->points[7] = cubic.anchor1Y();
                    }
                }
            }
        }
        if (lastCubic && firstCubic) {
            m_cubics.push_back(Cubic(lastCubic->anchor0X(), lastCubic->anchor0Y(), lastCubic->control0X(),
                lastCubic->control0Y(), lastCubic->control1X(), lastCubic->control1Y(), firstCubic->anchor0X(),
                firstCubic->anchor0Y()));
        } else {
            // Empty / 0-sized polygon.
            m_cubics.push_back(Cubic(centerX(), centerY(), centerX(), centerY(), centerX(), centerY(), centerX(), centerY()));
        }

        const Cubic* prevCubic = &m_cubics[m_cubics.size() - 1];
        for (const Cubic& cubic : m_cubics) {
            if (kmath::abs(cubic.anchor0X() - prevCubic->anchor1X()) > DistanceEpsilon ||
                kmath::abs(cubic.anchor0Y() - prevCubic->anchor1Y()) > DistanceEpsilon) {
                throw std::invalid_argument(
                    "RoundedPolygon must be contiguous, with the anchor points of all curves "
                    "matching the anchor points of the preceding and succeeding cubics");
            }
            prevCubic = &cubic;
        }
    }

    RoundedPolygon RoundedPolygon::transformed(const PointTransformer& f) const {
        Point center = m3shapes::transformed(m_center, f);
        std::vector<Feature> features;
        features.reserve(m_features.size());
        for (const Feature& feature : m_features) features.push_back(feature.transformed(f));
        return RoundedPolygon(std::move(features), center);
    }

    RoundedPolygon RoundedPolygon::normalized() const {
        auto bounds = calculateBounds();
        float width = bounds[2] - bounds[0];
        float height = bounds[3] - bounds[1];
        float side = std::max(width, height);
        // Center the shape if bounds are not a square
        float offsetX = (side - width) / 2 - bounds[0]; /* left */
        float offsetY = (side - height) / 2 - bounds[1]; /* top */
        return transformed([=](float x, float y) {
            return TransformResult((x + offsetX) / side, (y + offsetY) / side);
        });
    }

    std::array<float, 4> RoundedPolygon::calculateMaxBounds() const {
        float maxDistSquared = 0.f;
        for (const Cubic& cubic : m_cubics) {
            float anchorDistance = distanceSquared(cubic.anchor0X() - centerX(), cubic.anchor0Y() - centerY());
            Point middlePoint = cubic.pointOnCurve(.5f);
            float middleDistance = distanceSquared(middlePoint.x - centerX(), middlePoint.y - centerY());
            maxDistSquared = std::max(maxDistSquared, std::max(anchorDistance, middleDistance));
        }
        float distance = kmath::sqrt(maxDistSquared);
        return {centerX() - distance, centerY() - distance, centerX() + distance, centerY() + distance};
    }

    std::array<float, 4> RoundedPolygon::calculateBounds(bool approximate) const {
        std::array<float, 4> bounds{};
        float minX = KotlinFloatMaxValue;
        float minY = KotlinFloatMaxValue;
        // Kotlin's Float.MIN_VALUE (smallest positive float), as upstream
        float maxX = KotlinFloatMinValue;
        float maxY = KotlinFloatMinValue;
        for (const Cubic& cubic : m_cubics) {
            cubic.calculateBounds(bounds, approximate);
            minX = std::min(minX, bounds[0]);
            minY = std::min(minY, bounds[1]);
            maxX = std::max(maxX, bounds[2]);
            maxY = std::max(maxY, bounds[3]);
        }
        return {minX, minY, maxX, maxY};
    }

    RoundedPolygon RoundedPolygon::fromNumVertices(int numVertices, float radius, float centerX, float centerY,
        CornerRounding rounding, const std::vector<CornerRounding>& perVertexRounding) {
        return fromVertices(verticesFromNumVerts(numVertices, radius, centerX, centerY), rounding,
            perVertexRounding, centerX, centerY);
    }

    // TODO(performance): Update the map calls to more efficient code that doesn't allocate
    //  Iterators unnecessarily.
    RoundedPolygon RoundedPolygon::fromVertices(const std::vector<float>& vertices, CornerRounding rounding,
        const std::vector<CornerRounding>& perVertexRounding, float centerX, float centerY) {
        if (vertices.size() < 6) {
            throw std::invalid_argument("Polygons must have at least 3 vertices");
        }
        if (vertices.size() % 2 == 1) {
            throw std::invalid_argument("The vertices array should have even size");
        }
        if (!perVertexRounding.empty() && perVertexRounding.size() * 2 != vertices.size()) {
            throw std::invalid_argument(
                "perVertexRounding list should be either null or "
                "the same size as the number of vertices (vertices.size / 2)");
        }
        std::vector<std::vector<Cubic>> corners;
        const int n = static_cast<int>(vertices.size() / 2);
        std::vector<RoundedCorner> roundedCorners;
        roundedCorners.reserve(n);
        for (int i = 0; i < n; i++) {
            CornerRounding vtxRounding = perVertexRounding.empty() ? rounding : perVertexRounding[i];
            int prevIndex = ((i + n - 1) % n) * 2;
            int nextIndex = ((i + 1) % n) * 2;
            roundedCorners.emplace_back(
                Point(vertices[prevIndex], vertices[prevIndex + 1]),
                Point(vertices[i * 2], vertices[i * 2 + 1]),
                Point(vertices[nextIndex], vertices[nextIndex + 1]),
                vtxRounding);
        }

        // For each side, check if we have enough space to do the cuts needed, and if not split
        // the available space, first for round cuts, then for smoothing if there is space left.
        // Each element in this list is a pair, that represent how much we can do of the cut for
        // the given side (side i goes from corner i to corner i+1), the elements of the pair are:
        // first is how much we can use of expectedRoundCut, second how much of expectedCut
        std::vector<std::pair<float, float>> cutAdjusts;
        cutAdjusts.reserve(n);
        for (int ix = 0; ix < n; ix++) {
            float expectedRoundCut = roundedCorners[ix].expectedRoundCut + roundedCorners[(ix + 1) % n].expectedRoundCut;
            float expectedCut = roundedCorners[ix].expectedCut() + roundedCorners[(ix + 1) % n].expectedCut();
            float vtxX = vertices[ix * 2];
            float vtxY = vertices[ix * 2 + 1];
            float nextVtxX = vertices[((ix + 1) % n) * 2];
            float nextVtxY = vertices[((ix + 1) % n) * 2 + 1];
            float sideSize = distance(vtxX - nextVtxX, vtxY - nextVtxY);

            // Check expectedRoundCut first, and ensure we fulfill rounding needs first for
            // both corners before using space for smoothing
            if (expectedRoundCut > sideSize) {
                // Not enough room for fully rounding, see how much we can actually do.
                cutAdjusts.emplace_back(sideSize / expectedRoundCut, 0.f);
            } else if (expectedCut > sideSize) {
                // We can do full rounding, but not full smoothing.
                cutAdjusts.emplace_back(1.f, (sideSize - expectedRoundCut) / (expectedCut - expectedRoundCut));
            } else {
                // There is enough room for rounding & smoothing.
                cutAdjusts.emplace_back(1.f, 1.f);
            }
        }
        // Create and store list of beziers for each [potentially] rounded corner
        for (int i = 0; i < n; i++) {
            // allowedCuts[0] is for the side from the previous corner to this one,
            // allowedCuts[1] is for the side from this corner to the next one.
            float allowedCuts[2];
            for (int delta = 0; delta <= 1; delta++) {
                auto [roundCutRatio, cutRatio] = cutAdjusts[(i + n - 1 + delta) % n];
                allowedCuts[delta] =
                    roundedCorners[i].expectedRoundCut * roundCutRatio +
                    (roundedCorners[i].expectedCut() - roundedCorners[i].expectedRoundCut) * cutRatio;
            }
            corners.push_back(roundedCorners[i].getCubics(allowedCuts[0], allowedCuts[1]));
        }
        // Finally, store the calculated cubics. This includes all of the rounded corners
        // from above, along with new cubics representing the edges between those corners.
        std::vector<Feature> tempFeatures;
        tempFeatures.reserve(n * 2);
        for (int i = 0; i < n; i++) {
            // Note that these indices are for pairs of values (points), they need to be
            // doubled to access the xy values in the vertices float array
            int prevVtxIndex = (i + n - 1) % n;
            int nextVtxIndex = (i + 1) % n;
            Point currVertex = Point(vertices[i * 2], vertices[i * 2 + 1]);
            Point prevVertex = Point(vertices[prevVtxIndex * 2], vertices[prevVtxIndex * 2 + 1]);
            Point nextVertex = Point(vertices[nextVtxIndex * 2], vertices[nextVtxIndex * 2 + 1]);
            bool isConvex = convex(prevVertex, currVertex, nextVertex);
            tempFeatures.push_back(Feature::corner(corners[i], isConvex));
            tempFeatures.push_back(Feature::edge({Cubic::straightLine(
                corners[i].back().anchor1X(),
                corners[i].back().anchor1Y(),
                corners[(i + 1) % n].front().anchor0X(),
                corners[(i + 1) % n].front().anchor0Y())}));
        }

        Point c = (centerX == KotlinFloatMinValue || centerY == KotlinFloatMinValue)
                      ? calculateCenter(vertices)
                      : Point(centerX, centerY);
        return fromFeatures(std::move(tempFeatures), c.x, c.y);
    }

    RoundedPolygon RoundedPolygon::fromFeatures(std::vector<Feature> features, float centerX, float centerY) {
        if (features.size() < 2) throw std::invalid_argument("Polygons must have at least 2 features");
        std::vector<float> vertices;
        for (const Feature& feature : features) {
            for (const Cubic& cubic : feature.cubics()) {
                vertices.push_back(cubic.anchor0X());
                vertices.push_back(cubic.anchor0Y());
            }
        }
        float cX = std::isnan(centerX) ? calculateCenter(vertices).x : centerX;
        float cY = std::isnan(centerY) ? calculateCenter(vertices).y : centerY;
        return RoundedPolygon(std::move(features), Point(cX, cY));
    }

    // Calculates an estimated center position for the polygon, returning it. This function
    // should only be called if the center is not already calculated or provided. The Polygon
    // constructor which takes `numVertices` calculates its own center, since it knows exactly
    // where it is centered, at (0, 0).
    //
    // Note that this center will be transformed whenever the shape itself is transformed. Any
    // transforms that occur before the center is calculated will be taken into account
    // automatically since the center calculation is an average of the current location of all
    // cubic anchor points.
    Point calculateCenter(const std::vector<float>& vertices) {
        float cumulativeX = 0.f;
        float cumulativeY = 0.f;
        size_t index = 0;
        while (index < vertices.size()) {
            cumulativeX += vertices[index++];
            cumulativeY += vertices[index++];
        }
        const int count = static_cast<int>(vertices.size() / 2);
        return Point(cumulativeX / count, cumulativeY / count);
    }

} // namespace m3shapes
