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
#ifndef M3SHAPES_FEATUREMAPPING_H
#define M3SHAPES_FEATUREMAPPING_H

#include "feature.h"
#include "floatmapping.h"

#include <utility>
#include <vector>

namespace m3shapes {

    // A feature with its progress along the outline of the polygon it belongs to.
    struct ProgressableFeature {
            float progress;
            Feature feature;
    };

    using MeasuredFeatures = std::vector<ProgressableFeature>;

    // Creates a mapping between the "features" (rounded corners) of two shapes
    DoubleMapper featureMapper(const MeasuredFeatures& features1, const MeasuredFeatures& features2);

    // Returns a mapping of the features between features1 and features2. The return is a list of
    // pairs in which the first element is the progress of a feature in features1 and the second
    // element is the progress of the feature in features2 that we mapped it to. The list is sorted
    // by the first element. To do this:
    // 1) Compute the distance for all pairs of features in (features1 x features2),
    // 2) Sort ascending by by such distance
    // 3) Try to add them, from smallest distance to biggest, ensuring that: a) The features we are
    //    mapping haven't been mapped yet. b) We are not adding a crossing in the mapping. Since the
    //    mapping is sorted by the first element of each pair, this means that the second elements
    //    of each pair are monotonically increasing, except maybe one time (Counting all pair of
    //    consecutive elements, and the last element to first element).
    std::vector<std::pair<float, float>> doMapping(const std::vector<ProgressableFeature>& features1,
        const std::vector<ProgressableFeature>& features2);

    // Returns distance along overall shape between two Features on the two different shapes. This
    // information is used to determine how to map features (and the curves that make up those
    // features).
    float featureDistSquared(const Feature& f1, const Feature& f2);

    // TODO: b/378441547 - Move to explicit parameter / expose?
    Point featureRepresentativePoint(const Feature& feature);

} // namespace m3shapes

#endif // M3SHAPES_FEATUREMAPPING_H
