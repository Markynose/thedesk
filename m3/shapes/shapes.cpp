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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Shapes.kt
 */
#include "roundedpolygon.h"

#include <algorithm>

namespace m3shapes {

    namespace {

        std::vector<float> pillStarVerticesFromNumVerts(int numVerticesPerRadius, float width, float height,
            float innerRadius, float vertexSpacing, float startLocation, float centerX, float centerY) {
            // The general approach here is to get the perimeter of the underlying pill outline,
            // then the t value for each vertex as we walk that perimeter. This tells us where
            // on the outline to place that vertex, then we figure out where to place the vertex
            // depending on which "section" it is in. The possible sections are the vertical edges
            // on the sides, the circular sections on all four corners, or the horizontal edges
            // on the top and bottom. Note that either the vertical or horizontal edges will be
            // of length zero (whichever dimension is smaller gets only circular curvature for the
            // pill shape).
            float endcapRadius = std::min(width, height);
            float vSegLen = std::max(height - width, 0.f);
            float hSegLen = std::max(width - height, 0.f);
            float vSegHalf = vSegLen / 2;
            float hSegHalf = hSegLen / 2;
            // vertexSpacing is used to position the vertices on the end caps. The caller has the
            // choice of spacing the inner (0) or outer (1) vertices like those along the edges,
            // causing the other vertices to be either further apart (0) or closer (1). The default
            // is .5, which averages things. The magnitude of the inner and rounding parameters may
            // cause the caller to want a different value.
            float circlePerimeter = TwoPi * endcapRadius * interpolate(innerRadius, 1.f, vertexSpacing);
            // perimeter is circle perimeter plus horizontal and vertical sections of inner
            // rectangle, whether either (or even both) might be of length zero.
            float perimeter = 2 * hSegLen + 2 * vSegLen + circlePerimeter;

            // The sections array holds the t start values of that part of the outline. We use
            // these to determine which section a given vertex lies in, based on its t value, as
            // well as where in that section it lies.
            float sections[11];
            sections[0] = 0.f;
            sections[1] = vSegLen / 2;
            sections[2] = sections[1] + circlePerimeter / 4;
            sections[3] = sections[2] + hSegLen;
            sections[4] = sections[3] + circlePerimeter / 4;
            sections[5] = sections[4] + vSegLen;
            sections[6] = sections[5] + circlePerimeter / 4;
            sections[7] = sections[6] + hSegLen;
            sections[8] = sections[7] + circlePerimeter / 4;
            sections[9] = sections[8] + vSegLen / 2;
            sections[10] = perimeter;
            const int sectionsSize = 11;

            // "t" is the length along the entire pill outline for a given vertex. With vertices
            // spaced evenly along this contour, we can determine for any vertex where it should lie.
            float tPerVertex = perimeter / (2 * numVerticesPerRadius);
            // separate iteration for inner vs outer, unlike the other shapes, because
            // the vertices can lie in different quadrants so each needs their own calculation
            bool inner = false;
            // Increment section index as we walk around the pill contour with our increasing t
            // values
            int currSecIndex = 0;
            // secStart/End are used to determine how far along a given vertex is in the section
            // in which it lands
            float secStart = 0.f;
            float secEnd = sections[1];
            // t value is used to place each vertex. 0 is on the positive x axis,
            // moving into section 0 to begin with. startLocation, a value from 0 to 1, varies the
            // location anywhere on the perimeter of the shape
            float t = startLocation * perimeter;
            // The list of vertices to be returned
            std::vector<float> result(numVerticesPerRadius * 4);
            int arrayIndex = 0;
            Point rectBR = Point(hSegHalf, vSegHalf);
            Point rectBL = Point(-hSegHalf, vSegHalf);
            Point rectTL = Point(-hSegHalf, -vSegHalf);
            Point rectTR = Point(hSegHalf, -vSegHalf);
            // Each iteration through this loop uses the next t value as we walk around the shape
            for (int i = 0; i < numVerticesPerRadius * 2; i++) {
                // t could start (and end) after 0; extra boundedT logic makes sure it does the
                // right thing when crossing the boundar past 0 again
                float boundedT = kmath::rem(t, perimeter);
                if (boundedT < secStart) currSecIndex = 0;
                while (boundedT >= sections[(currSecIndex + 1) % sectionsSize]) {
                    currSecIndex = (currSecIndex + 1) % sectionsSize;
                    secStart = sections[currSecIndex];
                    secEnd = sections[(currSecIndex + 1) % sectionsSize];
                }
                // find t in section and its proportion of that section's total length
                float tInSection = boundedT - secStart;
                float tProportion = tInSection / (secEnd - secStart);

                // The vertex placement in a section varies depending on whether it is on one of
                // the semicircle endcaps or along one of the straight edges. For the endcaps, we
                // use tProportion to get the angle along that circular cap and add the starting
                // angle for that section. For the edges we use a straight linear calculation
                // given tProportion and the start/end t values for that edge.
                float currRadius = inner ? (endcapRadius * innerRadius) : endcapRadius;
                Point vertex;
                switch (currSecIndex) {
                    case 0:
                        vertex = Point(currRadius, tProportion * vSegHalf);
                        break;
                    case 1:
                        vertex = radialToCartesian(currRadius, tProportion * FloatPi / 2) + rectBR;
                        break;
                    case 2:
                        vertex = Point(hSegHalf - tProportion * hSegLen, currRadius);
                        break;
                    case 3:
                        vertex = radialToCartesian(currRadius, FloatPi / 2 + (tProportion * FloatPi / 2)) + rectBL;
                        break;
                    case 4:
                        vertex = Point(-currRadius, vSegHalf - tProportion * vSegLen);
                        break;
                    case 5:
                        vertex = radialToCartesian(currRadius, FloatPi + (tProportion * FloatPi / 2)) + rectTL;
                        break;
                    case 6:
                        vertex = Point(-hSegHalf + tProportion * hSegLen, -currRadius);
                        break;
                    case 7:
                        vertex = radialToCartesian(currRadius, FloatPi * 1.5f + (tProportion * FloatPi / 2)) + rectTR;
                        break;
                    default: // 8
                        vertex = Point(currRadius, -vSegHalf + tProportion * vSegHalf);
                        break;
                }
                result[arrayIndex++] = vertex.x + centerX;
                result[arrayIndex++] = vertex.y + centerY;
                t += tPerVertex;
                inner = !inner;
            }
            return result;
        }

        std::vector<float> starVerticesFromNumVerts(int numVerticesPerRadius, float radius, float innerRadius,
            float centerX, float centerY) {
            std::vector<float> result(numVerticesPerRadius * 4);
            int arrayIndex = 0;
            for (int i = 0; i < numVerticesPerRadius; i++) {
                Point vertex = radialToCartesian(radius, (FloatPi / numVerticesPerRadius * 2 * i));
                result[arrayIndex++] = vertex.x + centerX;
                result[arrayIndex++] = vertex.y + centerY;
                vertex = radialToCartesian(innerRadius, (FloatPi / numVerticesPerRadius * (2 * i + 1)));
                result[arrayIndex++] = vertex.x + centerX;
                result[arrayIndex++] = vertex.y + centerY;
            }
            return result;
        }

    } // namespace

    /**
     * Creates a circular shape, approximating the rounding of the shape around the underlying
     * polygon vertices.
     *
     * @param numVertices The number of vertices in the underlying polygon with which to
     *   approximate the circle, default value is 8
     * @param radius optional radius for the circle, default value is 1.0
     */
    RoundedPolygon RoundedPolygon::circle(int numVertices, float radius, float centerX, float centerY) {
        if (numVertices < 3) throw std::invalid_argument("Circle must have at least three vertices");

        // Half of the angle between two adjacent vertices on the polygon
        float theta = FloatPi / numVertices;
        // Radius of the underlying RoundedPolygon object given the desired radius of the circle
        float polygonRadius = radius / kmath::cos(theta);
        return fromNumVertices(numVertices, polygonRadius, centerX, centerY, CornerRounding(radius));
    }

    /**
     * Creates a rectangular shape with the given width/height around the given center. Optional
     * rounding parameters can be used to create a rounded rectangle instead.
     *
     * As with all [RoundedPolygon] objects, if this shape is created with default dimensions and
     * center, it is sized to fit within the 2x2 bounding box around a center of (0, 0) and will
     * need to be scaled and moved using [RoundedPolygon.transformed] to fit the intended area in a
     * UI.
     */
    RoundedPolygon RoundedPolygon::rectangle(float width, float height, CornerRounding rounding,
        const std::vector<CornerRounding>& perVertexRounding, float centerX, float centerY) {
        float left = centerX - width / 2;
        float top = centerY - height / 2;
        float right = centerX + width / 2;
        float bottom = centerY + height / 2;

        return fromVertices({right, bottom, left, bottom, left, top, right, top}, rounding,
            perVertexRounding, centerX, centerY);
    }

    /**
     * Creates a star polygon, which is like a regular polygon except every other vertex is on
     * either an inner or outer radius. The two radii specified in the constructor must both both
     * nonzero. If the radii are equal, the result will be a regular (not star) polygon with twice
     * the number of vertices specified in [numVerticesPerRadius].
     */
    RoundedPolygon RoundedPolygon::star(int numVerticesPerRadius, float radius, float innerRadius,
        CornerRounding rounding, const CornerRounding* innerRounding,
        const std::vector<CornerRounding>& perVertexRounding, float centerX, float centerY) {
        if (radius <= 0.f || innerRadius <= 0.f) {
            throw std::invalid_argument("Star radii must both be greater than 0");
        }
        if (innerRadius >= radius) {
            throw std::invalid_argument("innerRadius must be less than radius");
        }

        std::vector<CornerRounding> pvRounding = perVertexRounding;
        // If no per-vertex rounding supplied and caller asked for inner rounding,
        // create per-vertex rounding list based on supplied outer/inner rounding parameters
        if (pvRounding.empty() && innerRounding) {
            for (int i = 0; i < numVerticesPerRadius; i++) {
                pvRounding.push_back(rounding);
                pvRounding.push_back(*innerRounding);
            }
        }

        // Star polygon is just a polygon with all vertices supplied (where we generate
        // those vertices to be on the inner/outer radii)
        return fromVertices(starVerticesFromNumVerts(numVerticesPerRadius, radius, innerRadius, centerX, centerY),
            rounding, pvRounding, centerX, centerY);
    }

    /**
     * A pill shape consists of a rectangle shape bounded by two semicircles at either of the long
     * ends of the rectangle.
     */
    RoundedPolygon RoundedPolygon::pill(float width, float height, float smoothing, float centerX, float centerY) {
        if (!(width > 0.f && height > 0.f)) {
            throw std::invalid_argument("Pill shapes must have positive width and height");
        }

        float wHalf = width / 2;
        float hHalf = height / 2;
        return fromVertices(
            {
                wHalf + centerX,
                hHalf + centerY,
                -wHalf + centerX,
                hHalf + centerY,
                -wHalf + centerX,
                -hHalf + centerY,
                wHalf + centerX,
                -hHalf + centerY,
            },
            CornerRounding(std::min(wHalf, hHalf), smoothing), {}, centerX, centerY);
    }

    /**
     * A pillStar shape is like a [pill] except it has inner and outer radii along its pill-shaped
     * outline, just like a [star] has inner and outer radii along its circular outline. The
     * parameters for [pillStar] are similar to those of [star] except, like [pill], it has a
     * [width] and [height] to determine the general shape of the underlying pill. Also, there is
     * a subtle complication with the way that inner and outer vertices proceed along the circular
     * sections of the shape. The defaults for these parameters result in a shape where the inner
     * and outer vertices are evenly spaced along the bounds of the shape. The [vertexSpacing]
     * parameter allows the caller to change how the vertices are spaced along the curved parts.
     */
    RoundedPolygon RoundedPolygon::pillStar(float width, float height, int numVerticesPerRadius,
        float innerRadiusRatio, CornerRounding rounding, const CornerRounding* innerRounding,
        const std::vector<CornerRounding>& perVertexRounding, float vertexSpacing, float startLocation,
        float centerX, float centerY) {
        if (!(width > 0.f && height > 0.f)) {
            throw std::invalid_argument("Pill shapes must have positive width and height");
        }
        if (!(innerRadiusRatio > 0 && innerRadiusRatio <= 1)) {
            throw std::invalid_argument("innerRadius must be between 0 and 1");
        }

        std::vector<CornerRounding> pvRounding = perVertexRounding;
        // If no per-vertex rounding supplied and caller asked for inner rounding,
        // create per-vertex rounding list based on supplied outer/inner rounding parameters
        if (pvRounding.empty() && innerRounding) {
            for (int i = 0; i < numVerticesPerRadius; i++) {
                pvRounding.push_back(rounding);
                pvRounding.push_back(*innerRounding);
            }
        }

        return fromVertices(pillStarVerticesFromNumVerts(numVerticesPerRadius, width, height, innerRadiusRatio,
                                vertexSpacing, startLocation, centerX, centerY),
            rounding, pvRounding, centerX, centerY);
    }

} // namespace m3shapes
