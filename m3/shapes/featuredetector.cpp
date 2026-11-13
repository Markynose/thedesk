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
#include "featuredetector.h"

#include "utils.h"

namespace m3shapes {

    namespace {
        Cubic extend(const Cubic& a, const Cubic& b) {
            if (a.zeroLength()) {
                return Cubic(a.anchor0X(), a.anchor0Y(), b.control0X(), b.control0Y(), b.control1X(), b.control1Y(),
                    b.anchor1X(), b.anchor1Y());
            }
            return Cubic(a.anchor0X(), a.anchor0Y(), a.control0X(), a.control0Y(), a.control1X(), a.control1Y(),
                b.anchor1X(), b.anchor1Y());
        }
    } // namespace

    std::vector<Feature> detectFeatures(const std::vector<Cubic>& cubics) {
        if (cubics.empty()) return {};

        // TODO: b/372651969 Try different heuristics for corner grouping
        std::vector<Feature> result;
        Cubic current = cubics.front();

        // Do one roundabout in which (current == last, next == first) is the last iteration.
        // Just like a snowball, subsequent cubics that align to one feature merge until
        // the streak breaks, the result is added, and a new streak starts.
        for (size_t i = 0; i < cubics.size(); i++) {
            const Cubic& next = cubics[(i + 1) % cubics.size()];

            if (i < cubics.size() - 1 && alignsIshWith(current, next)) {
                current = extend(current, next);
                continue;
            }

            result.push_back(asFeature(current, next));

            if (!smoothesIntoIsh(current, next)) {
                result.push_back(asFeature(Cubic::empty(current.anchor1X(), current.anchor1Y()), next));
            }

            current = next;
        }
        return result;
    }

    Feature asFeature(const Cubic& cubic, const Cubic& next) {
        if (straightIsh(cubic)) return Feature::edge({cubic});
        return Feature::corner({cubic}, cubic.convexTo(next));
    }

    bool straightIsh(const Cubic& c) {
        return !c.zeroLength() &&
               collinearIsh(c.anchor0X(), c.anchor0Y(), c.anchor1X(), c.anchor1Y(), c.control0X(), c.control0Y(),
                   RelaxedDistanceEpsilon) &&
               collinearIsh(c.anchor0X(), c.anchor0Y(), c.anchor1X(), c.anchor1Y(), c.control1X(), c.control1Y(),
                   RelaxedDistanceEpsilon);
    }

    bool smoothesIntoIsh(const Cubic& c, const Cubic& next) {
        return collinearIsh(c.control1X(), c.control1Y(), next.control0X(), next.control0Y(), c.anchor1X(),
            c.anchor1Y(), RelaxedDistanceEpsilon);
    }

    bool alignsIshWith(const Cubic& c, const Cubic& next) {
        return (straightIsh(c) && straightIsh(next) && smoothesIntoIsh(c, next)) ||
               c.zeroLength() ||
               next.zeroLength();
    }

    namespace PolygonValidator {
        namespace {
            bool isCWOriented(const RoundedPolygon& polygon) {
                float signedArea = 0.0f;
                for (const Cubic& cubic : polygon.cubics()) {
                    signedArea += (cubic.anchor1X() - cubic.anchor0X()) * (cubic.anchor1Y() + cubic.anchor0Y());
                }
                return signedArea < 0;
            }

            RoundedPolygon fixCWOrientation(const RoundedPolygon& polygon) {
                std::vector<Feature> reversedFeatures;
                // Persist first feature to stay a Corner
                reversedFeatures.push_back(polygon.features().front().reversed());
                for (size_t i = polygon.features().size() - 1; i >= 1; i--) {
                    reversedFeatures.push_back(polygon.features()[i].reversed());
                }
                return RoundedPolygon(std::move(reversedFeatures), polygon.center());
            }
        } // namespace

        RoundedPolygon fix(const RoundedPolygon& polygon) {
            if (isCWOriented(polygon)) return polygon;
            return fixCWOrientation(polygon);
        }
    } // namespace PolygonValidator

} // namespace m3shapes
