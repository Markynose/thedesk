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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/PolygonMeasure.kt
 */
#include "polygonmeasure.h"

#include "utils.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace m3shapes {

    MeasuredPolygon::MeasuredPolygon(std::shared_ptr<const Measurer> measurer, std::vector<ProgressableFeature> features,
        const std::vector<Cubic>& cubics, const std::vector<float>& outlineProgress) :
        m_measurer(std::move(measurer)), m_features(std::move(features)) {
        if (outlineProgress.size() != cubics.size() + 1) {
            throw std::invalid_argument("Outline progress size is expected to be the cubics size + 1");
        }
        if (outlineProgress.front() != 0.f) {
            throw std::invalid_argument("First outline progress value is expected to be zero");
        }
        if (outlineProgress.back() != 1.f) {
            throw std::invalid_argument("Last outline progress value is expected to be one");
        }

        float startOutlineProgress = 0.f;
        for (size_t index = 0; index < cubics.size(); index++) {
            // Filter out "empty" cubics
            if ((outlineProgress[index + 1] - outlineProgress[index]) > DistanceEpsilon) {
                m_cubics.emplace_back(m_measurer, cubics[index], startOutlineProgress, outlineProgress[index + 1]);
                // The next measured cubic will start exactly where this one ends.
                startOutlineProgress = outlineProgress[index + 1];
            }
        }
        // We could have removed empty cubics at the end. Ensure the last measured cubic ends at 1f
        m_cubics.at(m_cubics.size() - 1).updateEndOutlineProgress(1.f);
    }

    MeasuredPolygon::MeasuredCubic::MeasuredCubic(std::shared_ptr<const Measurer> measurer, const Cubic& cubic,
        float startOutlineProgress, float endOutlineProgress) :
        cubic(cubic), m_measurer(std::move(measurer)) {
        if (!(endOutlineProgress >= startOutlineProgress)) {
            throw std::invalid_argument("endOutlineProgress is expected to be equal or greater than startOutlineProgress");
        }
        measuredSize = m_measurer->measureCubic(cubic);
        m_startOutlineProgress = startOutlineProgress;
        m_endOutlineProgress = endOutlineProgress;
    }

    void MeasuredPolygon::MeasuredCubic::updateProgressRange(float startOutlineProgress, float endOutlineProgress) {
        if (!(endOutlineProgress >= startOutlineProgress)) {
            throw std::invalid_argument("endOutlineProgress is expected to be equal or greater than startOutlineProgress");
        }
        m_startOutlineProgress = startOutlineProgress;
        m_endOutlineProgress = endOutlineProgress;
    }

    std::pair<MeasuredPolygon::MeasuredCubic, MeasuredPolygon::MeasuredCubic>
    MeasuredPolygon::MeasuredCubic::cutAtProgress(float cutOutlineProgress) const {
        // Floating point errors further up can cause cutOutlineProgress to land just
        // slightly outside of the start/end progress for this cubic, so we limit it
        // to those bounds to avoid further errors later
        float boundedCutOutlineProgress = std::clamp(cutOutlineProgress, m_startOutlineProgress, m_endOutlineProgress);
        float outlineProgressSize = m_endOutlineProgress - m_startOutlineProgress;
        float progressFromStart = boundedCutOutlineProgress - m_startOutlineProgress;

        // Note that in earlier parts of the computation, we have empty MeasuredCubics (cubics
        // with progressSize == 0f), but those cubics are filtered out before this method is
        // called.
        float relativeProgress = progressFromStart / outlineProgressSize;
        float t = m_measurer->findCubicCutPoint(cubic, relativeProgress * measuredSize);
        if (!(t >= 0.f && t <= 1.f)) throw std::logic_error("Cubic cut point is expected to be between 0 and 1");

        // c1/c2 are the two new cubics, then we return MeasuredCubics created from them
        auto [c1, c2] = cubic.split(t);
        return {
            MeasuredCubic(m_measurer, c1, m_startOutlineProgress, boundedCutOutlineProgress),
            MeasuredCubic(m_measurer, c2, boundedCutOutlineProgress, m_endOutlineProgress)};
    }

    MeasuredPolygon MeasuredPolygon::cutAndShift(float cuttingPoint) const {
        if (!(cuttingPoint >= 0.f && cuttingPoint <= 1.f)) {
            throw std::invalid_argument("Cutting point is expected to be between 0 and 1");
        }
        if (cuttingPoint < DistanceEpsilon) return *this;

        // Find the index of cubic we want to cut
        size_t targetIndex = m_cubics.size();
        for (size_t i = 0; i < m_cubics.size(); i++) {
            if (cuttingPoint >= m_cubics[i].startOutlineProgress() && cuttingPoint <= m_cubics[i].endOutlineProgress()) {
                targetIndex = i;
                break;
            }
        }
        const MeasuredCubic& target = m_cubics.at(targetIndex);
        // Cut the target cubic.
        // b1, b2 are two resulting cubics after cut
        auto [b1, b2] = target.cutAtProgress(cuttingPoint);

        // Construct the list of the cubics we need:
        // * The second part of the target cubic (after the cut)
        // * All cubics after the target, until the end + All cubics from the start, before the
        //   target cubic
        // * The first part of the target cubic (before the cut)
        std::vector<Cubic> retCubics = {b2.cubic};
        for (size_t i = 1; i < m_cubics.size(); i++) {
            retCubics.push_back(m_cubics[(i + targetIndex) % m_cubics.size()].cubic);
        }
        retCubics.push_back(b1.cubic);

        // Construct the array of outline progress.
        // For example, if we have 3 cubics with outline progress [0 .. 0.3], [0.3 .. 0.8] &
        // [0.8 .. 1.0], and we cut + shift at 0.6:
        // 0.  0123456789
        //     |--|--/-|-|
        // The outline progresses will start at 0 (the cutting point, that shifs to 0.0),
        // then 0.8 - 0.6 = 0.2, then 1 - 0.6 = 0.4, then 0.3 - 0.6 + 1 = 0.7,
        // then 1 (the cutting point again),
        // all together: (0.0, 0.2, 0.4, 0.7, 1.0)
        std::vector<float> retOutlineProgress;
        retOutlineProgress.reserve(m_cubics.size() + 2);
        for (size_t index = 0; index < m_cubics.size() + 2; index++) {
            if (index == 0) {
                retOutlineProgress.push_back(0.f);
            } else if (index == m_cubics.size() + 1) {
                retOutlineProgress.push_back(1.f);
            } else {
                size_t cubicIndex = (targetIndex + index - 1) % m_cubics.size();
                retOutlineProgress.push_back(positiveModulo(m_cubics[cubicIndex].endOutlineProgress() - cuttingPoint, 1.f));
            }
        }

        // Shift the feature's outline progress too.
        std::vector<ProgressableFeature> newFeatures;
        newFeatures.reserve(m_features.size());
        for (const auto& f : m_features) {
            newFeatures.push_back({positiveModulo(f.progress - cuttingPoint, 1.f), f.feature});
        }

        // Filter out all empty cubics (i.e. start and end anchor are (almost) the same point.)
        return MeasuredPolygon(m_measurer, std::move(newFeatures), retCubics, retOutlineProgress);
    }

    MeasuredPolygon MeasuredPolygon::measurePolygon(std::shared_ptr<const Measurer> measurer, const RoundedPolygon& polygon) {
        std::vector<Cubic> cubics;
        std::vector<std::pair<const Feature*, size_t>> featureToCubic;

        // Get the cubics from the polygon, at the same time, extract the features and keep a
        // reference to the representative cubic we will use.
        for (const Feature& feature : polygon.features()) {
            for (size_t cubicIndex = 0; cubicIndex < feature.cubics().size(); cubicIndex++) {
                if (feature.isCorner() && cubicIndex == feature.cubics().size() / 2) {
                    featureToCubic.emplace_back(&feature, cubics.size());
                }
                cubics.push_back(feature.cubics()[cubicIndex]);
            }
        }

        std::vector<float> measures;
        measures.reserve(cubics.size() + 1);
        measures.push_back(0.f);
        for (const Cubic& cubic : cubics) {
            float m = measurer->measureCubic(cubic);
            if (!(m >= 0.f)) throw std::logic_error("Measured cubic is expected to be greater or equal to zero");
            measures.push_back(measures.back() + m);
        }
        float totalMeasure = measures.back();

        // Equivalent to `measures.map { it / totalMeasure }` but without Iterator allocation.
        std::vector<float> outlineProgress;
        outlineProgress.reserve(measures.size());
        for (float m : measures) outlineProgress.push_back(m / totalMeasure);

        std::vector<ProgressableFeature> features;
        features.reserve(featureToCubic.size());
        for (const auto& [feature, ix] : featureToCubic) {
            features.push_back({positiveModulo((outlineProgress[ix] + outlineProgress[ix + 1]) / 2, 1.f), *feature});
        }

        return MeasuredPolygon(std::move(measurer), std::move(features), cubics, outlineProgress);
    }

    float LengthMeasurer::measureCubic(const Cubic& c) const {
        return closestProgressTo(c, std::numeric_limits<float>::infinity()).second;
    }

    float LengthMeasurer::findCubicCutPoint(const Cubic& c, float m) const {
        return closestProgressTo(c, m).first;
    }

    std::pair<float, float> LengthMeasurer::closestProgressTo(const Cubic& cubic, float threshold) const {
        float total = 0.f;
        float remainder = threshold;
        Point prev = Point(cubic.anchor0X(), cubic.anchor0Y());

        for (int i = 1; i <= segments; i++) {
            float progress = static_cast<float>(i) / segments;
            Point point = cubic.pointOnCurve(progress);
            float segment = (point - prev).getDistance();

            if (segment >= remainder) {
                return {progress - (1.0f - remainder / segment) / segments, threshold};
            }

            remainder -= segment;
            total += segment;
            prev = point;
        }

        return {1.0f, total};
    }

} // namespace m3shapes
