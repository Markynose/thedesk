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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/FloatMapping.kt
 */
#ifndef M3SHAPES_FLOATMAPPING_H
#define M3SHAPES_FLOATMAPPING_H

#include <utility>
#include <vector>

namespace m3shapes {

    // Checks if the given progress is in the given progress range, since progress is in the
    // [0..1) interval, and wraps, there is a special case when progressTo < progressFrom. For
    // example, if the progress range is 0.7 to 0.2, both 0.8 and 0.1 are inside and 0.5 is
    // outside.
    bool progressInRange(float progress, float progressFrom, float progressTo);

    // Maps from one set of progress values to another. This is used by DoubleMapper to retrieve
    // the value on one shape that maps to the appropriate value on the other.
    float linearMap(const std::vector<float>& xValues, const std::vector<float>& yValues, float x);

    /**
     * DoubleMapper creates mappings from values in the [0..1) source space to values in the
     * [0..1) target space, and back. This mapping is created given a finite list of
     * representative mappings, and this is extended to the whole interval by linear
     * interpolation, and wrapping around. For example, if we have mappings 0.2 to 0.5 and 0.4 to
     * 0.6, then 0.3 (which is in the middle of the source interval) will be mapped to 0.55 (the
     * middle of the targets for the interval), 0.21 will map to 0.505, and so on. As a more
     * complicated example, if we have mappings 0.1 to 0.9 and 0.3 to 0.2, then 0.2 maps to 0.05
     * and 0.0 maps to 0.65 (wrapping).
     */
    class DoubleMapper {
        public:
            explicit DoubleMapper(const std::vector<std::pair<float, float>>& mappings);

            float map(float x) const;
            float mapBack(float x) const;

            static const DoubleMapper& identity();

        private:
            std::vector<float> m_sourceValues;
            std::vector<float> m_targetValues;
    };

    // Verify that a list of progress values are all in the range [0.0, 1.0) and is monotonically
    // increasing, with the exception of maybe one time in which the progress wraps around, this
    // check needs to include all pairs of consecutive elements in the list plus the last to first
    // element pair.
    void validateProgress(const std::vector<float>& p);

    // Distance between two progress values.
    // Since progress wraps around, we consider a difference of 0.99 as a distance of 0.01
    float progressDistance(float p1, float p2);

} // namespace m3shapes

#endif // M3SHAPES_FLOATMAPPING_H
