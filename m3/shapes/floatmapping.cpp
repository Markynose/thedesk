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
#include "floatmapping.h"

#include "utils.h"

#include <algorithm>
#include <stdexcept>

namespace m3shapes {

    bool progressInRange(float progress, float progressFrom, float progressTo) {
        if (progressTo >= progressFrom) {
            return progress >= progressFrom && progress <= progressTo;
        } else {
            return progress >= progressFrom || progress <= progressTo;
        }
    }

    float linearMap(const std::vector<float>& xValues, const std::vector<float>& yValues, float x) {
        if (!(x >= 0.f && x <= 1.f)) throw std::invalid_argument("Invalid progress");
        const size_t size = xValues.size();
        size_t segmentStartIndex = size;
        for (size_t it = 0; it < size; it++) {
            if (progressInRange(x, xValues[it], xValues[(it + 1) % size])) {
                segmentStartIndex = it;
                break;
            }
        }
        if (segmentStartIndex == size) throw std::logic_error("linearMap: no segment contains the progress");
        size_t segmentEndIndex = (segmentStartIndex + 1) % size;
        float segmentSizeX = positiveModulo(xValues[segmentEndIndex] - xValues[segmentStartIndex], 1.f);
        float segmentSizeY = positiveModulo(yValues[segmentEndIndex] - yValues[segmentStartIndex], 1.f);
        float positionInSegment = segmentSizeX < 0.001f
                                      ? 0.5f
                                      : positiveModulo(x - xValues[segmentStartIndex], 1.f) / segmentSizeX;
        return positiveModulo(yValues[segmentStartIndex] + segmentSizeY * positionInSegment, 1.f);
    }

    DoubleMapper::DoubleMapper(const std::vector<std::pair<float, float>>& mappings) {
        m_sourceValues.reserve(mappings.size());
        m_targetValues.reserve(mappings.size());
        for (const auto& m : mappings) {
            m_sourceValues.push_back(m.first);
            m_targetValues.push_back(m.second);
        }
        // Both source values and target values should be monotonically increasing, with the
        // exception of maybe one time (since progress wraps around).
        validateProgress(m_sourceValues);
        validateProgress(m_targetValues);
    }

    float DoubleMapper::map(float x) const {
        return linearMap(m_sourceValues, m_targetValues, x);
    }

    float DoubleMapper::mapBack(float x) const {
        return linearMap(m_targetValues, m_sourceValues, x);
    }

    const DoubleMapper& DoubleMapper::identity() {
        // We need any 2 points in the (x, x) diagonal, with x in the [0, 1) range,
        // We spread them as much as possible to minimize float errors.
        static const DoubleMapper identity({{0.f, 0.f}, {0.5f, 0.5f}});
        return identity;
    }

    void validateProgress(const std::vector<float>& p) {
        float prev = p.back();
        int wraps = 0;
        for (size_t i = 0; i < p.size(); i++) {
            float curr = p[i];
            if (!(curr >= 0.f && curr < 1.f)) {
                throw std::invalid_argument("FloatMapping - Progress outside of range");
            }
            if (!(progressDistance(curr, prev) > DistanceEpsilon)) {
                throw std::invalid_argument("FloatMapping - Progress repeats a value");
            }
            if (curr < prev) {
                wraps++;
                if (wraps > 1) {
                    throw std::invalid_argument("FloatMapping - Progress wraps more than once");
                }
            }
            prev = curr;
        }
    }

    float progressDistance(float p1, float p2) {
        float it = kmath::abs(p1 - p2);
        return std::min(it, 1.f - it);
    }

} // namespace m3shapes
