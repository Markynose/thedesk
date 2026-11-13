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
#ifndef M3SHAPES_POLYGONMEASURE_H
#define M3SHAPES_POLYGONMEASURE_H

#include "cubic.h"
#include "featuremapping.h"
#include "roundedpolygon.h"

#include <memory>
#include <utility>
#include <vector>

namespace m3shapes {

    /**
     * Interface for measuring a cubic. Implementations can use whatever algorithm desired to
     * produce these measurement values.
     */
    class Measurer {
        public:
            virtual ~Measurer() = default;
            // Returns size of given cubic, according to however the implementation wants to
            // measure the size (angle, length, etc). It has to be greater or equal to 0.
            virtual float measureCubic(const Cubic& c) const = 0;
            // Given a cubic and a measure that should be between 0 and the value returned by
            // measureCubic (If not, it will be capped), finds the parameter t of the cubic at
            // which that measure is reached.
            virtual float findCubicCutPoint(const Cubic& c, float m) const = 0;
    };

    // Approximates the arc lengths of cubics by splitting the arc into segments and calculating
    // their sizes. The more segments, the more accurate the result will be to the true arc
    // length. The default implementation has at least 98.5% accuracy on the case of a circular
    // arc, which is the worst case for our standard shapes.
    class LengthMeasurer : public Measurer {
        public:
            float measureCubic(const Cubic& c) const override;
            float findCubicCutPoint(const Cubic& c, float m) const override;

        private:
            // The minimum number needed to achieve up to 98.5% accuracy from the true arc length
            // See PolygonMeasureTest.measureCircle
            static constexpr int segments = 3;
            std::pair<float, float> closestProgressTo(const Cubic& cubic, float threshold) const;
    };

    class MeasuredPolygon {
        public:
            // A wrapper around a cubic that also holds the measurement of the cubic and its
            // position along the outline of the polygon.
            class MeasuredCubic {
                public:
                    MeasuredCubic(std::shared_ptr<const Measurer> measurer, const Cubic& cubic,
                        float startOutlineProgress, float endOutlineProgress);

                    Cubic cubic;
                    float measuredSize;
                    float startOutlineProgress() const { return m_startOutlineProgress; }
                    float endOutlineProgress() const { return m_endOutlineProgress; }

                    void updateProgressRange(float startOutlineProgress, float endOutlineProgress);
                    void updateEndOutlineProgress(float endOutlineProgress) {
                        updateProgressRange(m_startOutlineProgress, endOutlineProgress);
                    }

                    // Cut this MeasuredCubic into two MeasuredCubics at the given outline progress
                    // value.
                    std::pair<MeasuredCubic, MeasuredCubic> cutAtProgress(float cutOutlineProgress) const;

                private:
                    std::shared_ptr<const Measurer> m_measurer;
                    float m_startOutlineProgress;
                    float m_endOutlineProgress;
            };

            // Finds the point in the input list of measured cubics that pass the given outline
            // progress, and generates a new MeasuredPolygon (equivalent to this), that starts at
            // that point. This usually means cutting the cubic that crosses the outline progress
            // (unless the cut is at one of its ends) For example, given outline progress 0.4f and
            // measured cubics on these outline progress ranges:
            //
            // c1 [0 -> 0.2] c2 [0.2 -> 0.5] c3 [0.5 -> 1.0]
            //
            // c2 will be cut in two, at the given outline progress, we can name these c2a [0.2 ->
            // 0.4] and c2b [0.4 -> 0.5]
            //
            // The return then will have measured cubics [c2b, c3, c1, c2a], and they will have
            // their outline progress ranges adjusted so the new list starts at 0. c2b [0 -> 0.1]
            // c3 [0.1 -> 0.6] c1 [0.6 -> 0.8] c2a [0.8 -> 1.0]
            MeasuredPolygon cutAndShift(float cuttingPoint) const;

            size_t size() const { return m_cubics.size(); }
            const MeasuredCubic& operator[](size_t index) const { return m_cubics[index]; }
            const std::vector<ProgressableFeature>& features() const { return m_features; }

            static MeasuredPolygon measurePolygon(std::shared_ptr<const Measurer> measurer, const RoundedPolygon& polygon);

        private:
            MeasuredPolygon(std::shared_ptr<const Measurer> measurer, std::vector<ProgressableFeature> features,
                const std::vector<Cubic>& cubics, const std::vector<float>& outlineProgress);

            std::shared_ptr<const Measurer> m_measurer;
            std::vector<MeasuredCubic> m_cubics;
            std::vector<ProgressableFeature> m_features;
    };

} // namespace m3shapes

#endif // M3SHAPES_POLYGONMEASURE_H
