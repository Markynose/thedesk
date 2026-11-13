/*
 * Copyright 2024 The Android Open Source Project
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
 * Sources: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/FeatureDetector.kt
 *          androidx graphics/graphics-shapes/.../androidx/graphics/shapes/PolygonValidation.kt
 */
#ifndef M3SHAPES_FEATUREDETECTOR_H
#define M3SHAPES_FEATUREDETECTOR_H

#include "feature.h"
#include "roundedpolygon.h"

#include <vector>

namespace m3shapes {

    /*
     * Convert cubics to Features in a 1:1 mapping of [Cubic.asFeature] unless
     * - two subsequent cubics are not continuous, in which case an empty corner needs to be added
     *   in between. Example for C1, C2: /C1\/C2\ -> /C1\|/C2\
     * - multiple subsequent cubics can be expressed as a single feature. Example for C1, C2:
     *   --C1----C2-- -> -----E----
     * One more example of all those operations: --C1----C2--\/C3\/C4\ -> -----E---|/C3\|/C4\
     */
    std::vector<Feature> detectFeatures(const std::vector<Cubic>& cubics);

    // Create a feature from this cubic: an edge if it is straight-ish, else a corner
    Feature asFeature(const Cubic& cubic, const Cubic& next);
    bool straightIsh(const Cubic& cubic);
    bool smoothesIntoIsh(const Cubic& cubic, const Cubic& next);
    bool alignsIshWith(const Cubic& cubic, const Cubic& next);

    // PolygonValidation.kt: validates and fixes a polygon's orientation (makes it clockwise).
    // Upstream keeps this internal and unused by the shapes above; ported for completeness.
    namespace PolygonValidator {
        RoundedPolygon fix(const RoundedPolygon& polygon);
    }

} // namespace m3shapes

#endif // M3SHAPES_FEATUREDETECTOR_H
