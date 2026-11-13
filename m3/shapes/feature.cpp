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
 * Source: androidx graphics/graphics-shapes/.../androidx/graphics/shapes/Features.kt
 */
#include "feature.h"

#include "utils.h"

namespace m3shapes {

    Feature::Feature(Kind kind, std::vector<Cubic> cubics, bool convex) :
        m_kind(kind), m_cubics(std::move(cubics)), m_convex(convex) {}

    Feature Feature::edge(std::vector<Cubic> cubics) {
        return Feature(Kind::Edge, std::move(cubics), false);
    }

    Feature Feature::corner(std::vector<Cubic> cubics, bool convex) {
        return Feature(Kind::Corner, std::move(cubics), convex);
    }

    Feature Feature::buildIgnorableFeature(const std::vector<Cubic>& cubics) {
        return validated(edge(cubics));
    }

    Feature Feature::buildEdge(const Cubic& cubic) {
        return edge({cubic});
    }

    Feature Feature::buildConvexCorner(const std::vector<Cubic>& cubics) {
        return validated(corner(cubics, true));
    }

    Feature Feature::buildConcaveCorner(const std::vector<Cubic>& cubics) {
        return validated(corner(cubics, false));
    }

    Feature Feature::validated(Feature feature) {
        if (feature.m_cubics.empty()) throw std::invalid_argument("Features need at least one cubic.");
        if (!isContinuous(feature)) {
            throw std::invalid_argument(
                "Feature must be continuous, with the anchor points of all cubics "
                "matching the anchor points of the preceding and succeeding cubics");
        }
        return feature;
    }

    bool Feature::isContinuous(const Feature& feature) {
        const Cubic* prevCubic = &feature.m_cubics.front();
        for (size_t index = 1; index < feature.m_cubics.size(); index++) {
            const Cubic& cubic = feature.m_cubics[index];
            if (kmath::abs(cubic.anchor0X() - prevCubic->anchor1X()) > DistanceEpsilon ||
                kmath::abs(cubic.anchor0Y() - prevCubic->anchor1Y()) > DistanceEpsilon) {
                return false;
            }
            prevCubic = &cubic;
        }
        return true;
    }

    Feature Feature::transformed(const PointTransformer& f) const {
        std::vector<Cubic> cubics;
        cubics.reserve(m_cubics.size());
        for (const Cubic& c : m_cubics) cubics.push_back(c.transformed(f));
        return Feature(m_kind, std::move(cubics), m_convex);
    }

    Feature Feature::reversed() const {
        std::vector<Cubic> reversedCubics;
        reversedCubics.reserve(m_cubics.size());
        for (auto it = m_cubics.rbegin(); it != m_cubics.rend(); ++it) reversedCubics.push_back(it->reverse());
        if (m_kind == Kind::Edge) return edge(std::move(reversedCubics));
        // TODO: b/369320447 - Revert flag negation when [RoundedPolygon] ignores orientation
        // for setting the flag
        return corner(std::move(reversedCubics), !m_convex);
    }

} // namespace m3shapes
