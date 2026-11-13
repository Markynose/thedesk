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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/FeatureMapping.kt
 */
#include "featuremapping.h"

#include "utils.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace m3shapes {

    namespace {

        struct DistanceVertex {
                float distance;
                // Indices into the (filtered) features1 / features2 lists. Kotlin compares
                // ProgressableFeature data classes by (progress, feature identity), which is
                // the same as comparing their positions in these lists.
                size_t f1;
                size_t f2;
        };

        const std::vector<std::pair<float, float>> IdentityMapping = {{0.f, 0.f}, {0.5f, 0.5f}};

        class MappingHelper {
            public:
                // List of mappings from progress in the start shape to progress in the end shape.
                // We keep this list sorted by the first element.
                std::vector<std::pair<float, float>> mapping;

                MappingHelper(const std::vector<ProgressableFeature>& features1,
                    const std::vector<ProgressableFeature>& features2) :
                    m_features1(features1), m_features2(features2) {}

                void addMapping(size_t f1Index, size_t f2Index) {
                    // We don't want to map the same feature twice.
                    if (m_usedF1.contains(f1Index) || m_usedF2.contains(f2Index)) return;

                    const ProgressableFeature& f1 = m_features1[f1Index];
                    const ProgressableFeature& f2 = m_features2[f2Index];

                    // Ret is sorted, find where we need to insert this new mapping.
                    auto lb = std::lower_bound(mapping.begin(), mapping.end(), f1.progress,
                        [](const std::pair<float, float>& m, float value) { return m.first < value; });
                    if (lb != mapping.end() && lb->first == f1.progress) {
                        throw std::invalid_argument("There can't be two features with the same progress");
                    }
                    const int insertionIndex = static_cast<int>(lb - mapping.begin());
                    const int n = static_cast<int>(mapping.size());

                    // We can always add the first 1 element
                    if (n >= 1) {
                        auto [before1, before2] = mapping[(insertionIndex + n - 1) % n];
                        auto [after1, after2] = mapping[insertionIndex % n];

                        // We don't want features that are way too close to each other, that will
                        // make the DoubleMapper unstable
                        if (progressDistance(f1.progress, before1) < DistanceEpsilon ||
                            progressDistance(f1.progress, after1) < DistanceEpsilon ||
                            progressDistance(f2.progress, before2) < DistanceEpsilon ||
                            progressDistance(f2.progress, after2) < DistanceEpsilon) {
                            return;
                        }

                        // When we have 2 or more elements, we need to ensure we are not adding
                        // extra crossings.
                        if (n > 1 && !progressInRange(f2.progress, before2, after2)) return;
                    }

                    // All good, we can add the mapping.
                    mapping.insert(mapping.begin() + insertionIndex, {f1.progress, f2.progress});
                    m_usedF1.insert(f1Index);
                    m_usedF2.insert(f2Index);
                }

            private:
                const std::vector<ProgressableFeature>& m_features1;
                const std::vector<ProgressableFeature>& m_features2;
                // Which features in the start shape have we used and which in the end shape.
                std::set<size_t> m_usedF1;
                std::set<size_t> m_usedF2;
        };

    } // namespace

    DoubleMapper featureMapper(const MeasuredFeatures& features1, const MeasuredFeatures& features2) {
        // We only use corners for this mapping.
        std::vector<ProgressableFeature> filteredFeatures1;
        for (const auto& f : features1) {
            if (f.feature.isCorner()) filteredFeatures1.push_back(f);
        }
        std::vector<ProgressableFeature> filteredFeatures2;
        for (const auto& f : features2) {
            if (f.feature.isCorner()) filteredFeatures2.push_back(f);
        }

        auto featureProgressMapping = doMapping(filteredFeatures1, filteredFeatures2);
        return DoubleMapper(featureProgressMapping);
    }

    std::vector<std::pair<float, float>> doMapping(const std::vector<ProgressableFeature>& features1,
        const std::vector<ProgressableFeature>& features2) {
        std::vector<DistanceVertex> distanceVertexList;
        for (size_t i1 = 0; i1 < features1.size(); i1++) {
            for (size_t i2 = 0; i2 < features2.size(); i2++) {
                float d = featureDistSquared(features1[i1].feature, features2[i2].feature);
                if (d != KotlinFloatMaxValue) distanceVertexList.push_back({d, i1, i2});
            }
        }
        // Kotlin's sortedBy is stable
        std::stable_sort(distanceVertexList.begin(), distanceVertexList.end(),
            [](const DistanceVertex& a, const DistanceVertex& b) { return a.distance < b.distance; });

        // Special cases.
        if (distanceVertexList.empty()) return IdentityMapping;
        if (distanceVertexList.size() == 1) {
            const auto& it = distanceVertexList.front();
            float f1 = features1[it.f1].progress;
            float f2 = features2[it.f2].progress;
            return {{f1, f2}, {kmath::rem(f1 + 0.5f, 1.f), kmath::rem(f2 + 0.5f, 1.f)}};
        }

        MappingHelper helper(features1, features2);
        for (const auto& it : distanceVertexList) helper.addMapping(it.f1, it.f2);
        return helper.mapping;
    }

    float featureDistSquared(const Feature& f1, const Feature& f2) {
        // TODO: We might want to enable concave-convex matching in some situations. If so, the
        //  approach below will not work
        if (f1.isCorner() && f2.isCorner() && f1.convex() != f2.convex()) {
            // Simple hack to force all features to map only to features of the same concavity, by
            // returning an infinitely large distance in that case
            return KotlinFloatMaxValue;
        }
        return (featureRepresentativePoint(f1) - featureRepresentativePoint(f2)).getDistanceSquared();
    }

    Point featureRepresentativePoint(const Feature& feature) {
        float x = (feature.cubics().front().anchor0X() + feature.cubics().back().anchor1X()) / 2.f;
        float y = (feature.cubics().front().anchor0Y() + feature.cubics().back().anchor1Y()) / 2.f;
        return Point(x, y);
    }

} // namespace m3shapes
