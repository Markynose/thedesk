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
#include "morph.h"

#include "featuremapping.h"
#include "polygonmeasure.h"
#include "utils.h"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace m3shapes {

    Morph::Morph(const RoundedPolygon& start, const RoundedPolygon& end) :
        m_start(start), m_end(end), m_morphMatch(match(start, end)) {}

    std::array<float, 4> Morph::calculateBounds(bool approximate) const {
        auto bounds = m_start.calculateBounds(approximate);
        float minX = bounds[0];
        float minY = bounds[1];
        float maxX = bounds[2];
        float maxY = bounds[3];
        bounds = m_end.calculateBounds(approximate);
        bounds[0] = std::min(minX, bounds[0]);
        bounds[1] = std::min(minY, bounds[1]);
        bounds[2] = std::max(maxX, bounds[2]);
        bounds[3] = std::max(maxY, bounds[3]);
        return bounds;
    }

    std::array<float, 4> Morph::calculateMaxBounds() const {
        auto bounds = m_start.calculateMaxBounds();
        float minX = bounds[0];
        float minY = bounds[1];
        float maxX = bounds[2];
        float maxY = bounds[3];
        bounds = m_end.calculateMaxBounds();
        bounds[0] = std::min(minX, bounds[0]);
        bounds[1] = std::min(minY, bounds[1]);
        bounds[2] = std::max(maxX, bounds[2]);
        bounds[3] = std::max(maxY, bounds[3]);
        return bounds;
    }

    std::vector<Cubic> Morph::asCubics(float progress) const {
        std::vector<Cubic> result;
        result.reserve(m_morphMatch.size());
        // The first/last mechanism here ensures that the final anchor point in the shape
        // exactly matches the first anchor point. There can be rendering artifacts introduced
        // by those points being slightly off, even by much less than a pixel
        std::optional<Cubic> firstCubic;
        std::optional<Cubic> lastCubic;
        for (const auto& [first, second] : m_morphMatch) {
            std::array<float, 8> pts;
            for (int it = 0; it < 8; it++) pts[it] = interpolate(first.points[it], second.points[it], progress);
            Cubic cubic(pts);
            if (!firstCubic) firstCubic = cubic;
            if (lastCubic) result.push_back(*lastCubic);
            lastCubic = cubic;
        }
        if (lastCubic && firstCubic) {
            result.push_back(Cubic(lastCubic->anchor0X(), lastCubic->anchor0Y(), lastCubic->control0X(),
                lastCubic->control0Y(), lastCubic->control1X(), lastCubic->control1Y(), firstCubic->anchor0X(),
                firstCubic->anchor0Y()));
        }
        return result;
    }

    /**
     * [match], called at Morph construction time, creates the structure used to animate between
     * the start and end shapes. The technique is to match geometry (curves) between the shapes
     * when and where possible, and to create new/placeholder curves when necessary (when one of
     * the shapes has more curves than the other). The result is a list of pairs of Cubic curves.
     * Those curves are the matched pairs: the first of each pair holds the geometry of the start
     * shape, the second holds the geometry for the end shape. Changing the progress of a Morph
     * object simply interpolates between all pairs of curves for the morph shape.
     *
     * Curves on both shapes are matched by running the [Measurer] to determine where the points
     * are in each shape (proportionally, along the outline), and then running [featureMapper]
     * which decides how to map (match) all of the curves with each other.
     */
    std::vector<std::pair<Cubic, Cubic>> Morph::match(const RoundedPolygon& p1, const RoundedPolygon& p2) {
        // Measure polygons, returns lists of measured cubics for each polygon, which
        // we then use to match start/end curves
        auto measurer = std::make_shared<const LengthMeasurer>();
        MeasuredPolygon measuredPolygon1 = MeasuredPolygon::measurePolygon(measurer, p1);
        MeasuredPolygon measuredPolygon2 = MeasuredPolygon::measurePolygon(measurer, p2);

        // features1 and 2 will contain the list of corners (just the inner circular curve)
        // along with the progress at the middle of those corners. These measurement values
        // are then used to compare and match between the two polygons
        const auto& features1 = measuredPolygon1.features();
        const auto& features2 = measuredPolygon2.features();

        // Map features: doubleMapper is the result of mapping the features in each shape to the
        // closest feature in the other shape.
        // Given a progress in one of the shapes it can be used to find the corresponding
        // progress in the other shape (in both directions)
        DoubleMapper doubleMapper = featureMapper(features1, features2);

        // cut point on poly2 is the mapping of the 0 point on poly1
        float polygon2CutPoint = doubleMapper.map(0.f);

        // Cut and rotate.
        // Polygons start at progress 0, and the featureMapper has decided that we want to match
        // progress 0 in the first polygon to `polygon2CutPoint` on the second polygon.
        // So we need to cut the second polygon there and "rotate it", so as we walk through
        // both polygons we can find the matching.
        // The resulting bs1/2 are MeasuredPolygons, whose MeasuredCubics start from
        // outlineProgress=0 and increasing until outlineProgress=1
        const MeasuredPolygon& bs1 = measuredPolygon1;
        MeasuredPolygon bs2 = measuredPolygon2.cutAndShift(polygon2CutPoint);

        // Match
        // Now we can compare the two lists of measured cubics and create a list of pairs
        // of cubics [ret], which are the start/end curves that represent the Morph object
        // and the start and end shapes, and which can be interpolated to animate the
        // between those shapes.
        std::vector<std::pair<Cubic, Cubic>> ret;
        using MeasuredCubic = MeasuredPolygon::MeasuredCubic;
        auto getOrNull = [](const MeasuredPolygon& p, size_t index) -> std::optional<MeasuredCubic> {
            if (index < p.size()) return p[index];
            return std::nullopt;
        };
        // i1/i2 are the indices of the current cubic on the start (1) and end (2) shapes
        size_t i1 = 0;
        size_t i2 = 0;
        // b1, b2 are the current measured cubic for each polygon
        std::optional<MeasuredCubic> b1 = getOrNull(bs1, i1++);
        std::optional<MeasuredCubic> b2 = getOrNull(bs2, i2++);
        // Iterate until all curves are accounted for and matched
        while (b1 && b2) {
            // Progresses are in shape1's perspective
            // b1a, b2a are ending progress values of current measured cubics in [0,1] range
            float b1a = (i1 == bs1.size()) ? 1.f : b1->endOutlineProgress();
            float b2a = (i2 == bs2.size())
                            ? 1.f
                            : doubleMapper.mapBack(positiveModulo(b2->endOutlineProgress() + polygon2CutPoint, 1.f));
            float minb = std::min(b1a, b2a);
            // min b is the progress at which the curve that ends first ends.
            // If both curves ends roughly there, no cutting is needed, we have a match.
            // If one curve extends beyond, we need to cut it.
            std::optional<MeasuredCubic> seg1, newb1;
            if (b1a > minb + AngleEpsilon) {
                auto cut = b1->cutAtProgress(minb);
                seg1 = cut.first;
                newb1 = cut.second;
            } else {
                seg1 = *b1;
                newb1 = getOrNull(bs1, i1++);
            }
            std::optional<MeasuredCubic> seg2, newb2;
            if (b2a > minb + AngleEpsilon) {
                auto cut = b2->cutAtProgress(positiveModulo(doubleMapper.map(minb) - polygon2CutPoint, 1.f));
                seg2 = cut.first;
                newb2 = cut.second;
            } else {
                seg2 = *b2;
                newb2 = getOrNull(bs2, i2++);
            }
            ret.emplace_back(seg1->cubic, seg2->cubic);
            b1 = newb1;
            b2 = newb2;
        }
        if (b1 || b2) throw std::logic_error("Expected both Polygon's Cubic to be fully matched");

        return ret;
    }

} // namespace m3shapes
