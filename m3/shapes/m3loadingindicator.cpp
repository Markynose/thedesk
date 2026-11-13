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
 * Sources: androidx compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/
 *            LoadingIndicator.kt, tokens/LoadingIndicatorTokens.kt
 *          androidx compose/animation/animation-core/src/commonMain/kotlin/androidx/compose/animation/core/
 *            SpringSimulation.kt (Copyright 2019), SpringEstimation.kt (Copyright 2020),
 *            FloatAnimationSpec.kt (Copyright 2019)
 */
#include "m3loadingindicator.h"

#include "materialshapes.h"
#include "shapepath.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>

using m3shapes::Morph;
using m3shapes::RoundedPolygon;

namespace {

    // ---------------------------------------------------------------------------------------
    // Compose spring maths (animation-core)
    // ---------------------------------------------------------------------------------------

    // SpringSimulation.updateValuesSeconds(), value only
    float springValue(float finalPosition, float stiffness, float dampingRatio, float lastDisplacement,
        float lastVelocity, double deltaTSeconds) {
        const double naturalFreq = std::sqrt(static_cast<double>(stiffness));
        const float adjustedDisplacement = lastDisplacement - finalPosition;
        const double dampingRatioSquared = dampingRatio * static_cast<double>(dampingRatio);
        const double r = -dampingRatio * naturalFreq;

        double displacement;
        if (dampingRatio > 1) {
            // Over damping
            double s = naturalFreq * std::sqrt(dampingRatioSquared - 1);
            double gammaPlus = r + s;
            double gammaMinus = r - s;

            // Overdamped
            double coeffB = (gammaMinus * adjustedDisplacement - lastVelocity) / (gammaMinus - gammaPlus);
            double coeffA = adjustedDisplacement - coeffB;
            displacement = (coeffA * std::exp(gammaMinus * deltaTSeconds) + coeffB * std::exp(gammaPlus * deltaTSeconds));
        } else if (dampingRatio == 1.0f) {
            // Critically damped
            double coeffA = adjustedDisplacement;
            double coeffB = lastVelocity + naturalFreq * adjustedDisplacement;
            double nFdT = -naturalFreq * deltaTSeconds;
            displacement = (coeffA + coeffB * deltaTSeconds) * std::exp(nFdT);
        } else {
            double dampedFreq = naturalFreq * std::sqrt(1 - dampingRatioSquared);
            // Underdamped
            double cosCoeff = adjustedDisplacement;
            double sinCoeff = ((1 / dampedFreq) * (((-r * adjustedDisplacement) + lastVelocity)));
            double dFdT = dampedFreq * deltaTSeconds;
            displacement = (std::exp(r * deltaTSeconds) * ((cosCoeff * std::cos(dFdT) + sinCoeff * std::sin(dFdT))));
        }
        return static_cast<float>(displacement + finalPosition);
    }

    double iterateNewtonsMethod(double x, auto fn, auto fnPrime) {
        return x - fn(x) / fnPrime(x);
    }

    double estimateUnderDamped(double firstRootReal, double firstRootImaginary, double p0, double v0, double delta) {
        double r = firstRootReal;
        double c1 = p0;
        double c2 = (v0 - r * c1) / firstRootImaginary;
        double c = std::sqrt(c1 * c1 + c2 * c2);
        return std::log(delta / c) / r;
    }

    double estimateCriticallyDamped(double firstRootReal, double p0, double v0, double delta) {
        double r = firstRootReal;
        double c1 = p0;
        double c2 = v0 - r * c1;

        // For our initial guess, determine the max t of c_1*e^(r*t) = delta and
        // c_2*t*e^(r*t) = delta
        double t1 = std::log(std::abs(delta / c1)) / r;
        double t2;
        {
            // Application of Lambert's W function to solve te^t
            double guess = std::log(std::abs(delta / c2));
            double t = guess;
            for (int i = 0; i <= 5; i++) {
                t = (guess - std::log(std::abs(t / r)));
            }
            t2 = t / r;
        }
        double tCurr;
        if (!std::isfinite(t1)) tCurr = t2;
        else if (!std::isfinite(t2)) tCurr = t1;
        else tCurr = std::max(t1, t2);

        // Calculate the inflection time. This is important if the inflection is in t > 0
        double tInflection = -(r * c1 + c2) / (r * c2);
        double xInflection = c1 * std::exp(r * tInflection) + c2 * tInflection * std::exp(r * tInflection);

        // For inflection that does not exist in real time, we always solve for x(t)=delta. Note
        // the system is manipulated such that p0 is always positive.
        double signedDelta;
        if (std::isnan(tInflection) || tInflection <= 0.0) {
            signedDelta = -delta;
        } else if (tInflection > 0.0 && -xInflection < delta) {
            // In this scenario the first crossing with the threshold is to be found. Note that
            // the inflection does not exceed delta. As such, we search from the left.
            if (c2 < 0 && c1 > 0) {
                tCurr = 0.0;
            }
            signedDelta = -delta;
        } else {
            // In this scenario there are three total crossings of the threshold, once from
            // above, and then once when the inflection exceeds the threshold and then one last
            // one when x(t) finally decays to zero. The point of determining concavity is to
            // find the final crossing.
            //
            // By finding a point between when concavity changes, and when the inflection point
            // is, Newton's method will always converge onto the rightmost point (in this case),
            // the one that we are interested in.
            tCurr = -(2.0 / r) - (c1 / c2);
            signedDelta = delta;
        }

        double tDelta = std::numeric_limits<double>::max();
        int iterations = 0;
        while (tDelta > 0.001 && iterations < 100) {
            iterations++;
            double tLast = tCurr;
            tCurr = iterateNewtonsMethod(
                tCurr,
                [&](double t) { return (c1 + c2 * t) * std::exp(r * t) + signedDelta; },
                [&](double t) { return (c2 * (r * t + 1) + c1 * r) * std::exp(r * t); });
            tDelta = std::abs(tLast - tCurr);
        }

        return tCurr;
    }

    double estimateOverDamped(double firstRootReal, double secondRootReal, double p0, double v0, double delta) {
        double r1 = firstRootReal;
        double r2 = secondRootReal;
        double c2 = (r1 * p0 - v0) / (r1 - r2);
        double c1 = p0 - c2;

        // For our initial guess, determine the max t of c_1*e^(r_1*t) = delta and
        // c_2*e^(r_2*t) = delta
        double t1 = std::log(std::abs(delta / c1)) / r1;
        double t2 = std::log(std::abs(delta / c2)) / r2;

        double tCurr;
        if (!std::isfinite(t1)) tCurr = t2;
        else if (!std::isfinite(t2)) tCurr = t1;
        else tCurr = std::max(t1, t2);

        // Calculate the inflection time. This is important if the inflection is in t > 0
        double tInflection = std::log((c1 * r1) / (-c2 * r2)) / (r2 - r1);
        auto xInflection = [&]() { return c1 * std::exp(r1 * tInflection) + c2 * std::exp(r2 * tInflection); };

        // For inflection that does not exist in real time, we always solve for x(t)=delta. Note
        // the system is manipulated such that p0 is always positive.
        double signedDelta;
        if (std::isnan(tInflection) || tInflection <= 0.0) {
            signedDelta = -delta;
        } else if (tInflection > 0.0 && -xInflection() < delta) {
            // In this scenario the first crossing with the threshold is to be found. Note that
            // the inflection does not exceed delta. As such, we search from the left.
            if (c2 > 0.0 && c1 < 0.0) {
                tCurr = 0.0;
            }
            signedDelta = -delta;
        } else {
            // In this scenario there are three total crossings of the threshold, once from
            // above, and then once when the inflection exceeds the threshold and then one last
            // one when x(t) finally decays to zero. The point of determining concavity is to
            // find the final crossing.
            //
            // By finding a point between when concavity changes, and when the inflection point
            // is, Newton's method will always converge onto the rightmost point (in this case),
            // the one that we are interested in.
            tCurr = std::log(-(c2 * r2 * r2) / (c1 * r1 * r1)) / (r1 - r2);
            signedDelta = delta;
        }

        // For a good initial guess, simply return
        if (std::abs(c1 * r1 * std::exp(r1 * tCurr) + c2 * r2 * std::exp(r2 * tCurr)) < 0.0001) {
            return tCurr;
        }

        double tDelta = std::numeric_limits<double>::max();
        // Cap iterations for safety - Experimentally this method takes <= 5 iterations
        int iterations = 0;
        while (tDelta > 0.001 && iterations < 100) {
            iterations++;
            double tLast = tCurr;
            tCurr = iterateNewtonsMethod(
                tCurr,
                [&](double t) { return c1 * std::exp(r1 * t) + c2 * std::exp(r2 * t) + signedDelta; },
                [&](double t) { return c1 * r1 * std::exp(r1 * t) + c2 * r2 * std::exp(r2 * t); });
            tDelta = std::abs(tLast - tCurr);
        }

        return tCurr;
    }

    double estimateDurationSeconds(double firstRootReal, double firstRootImaginary, double secondRootReal,
        double dampingRatio, double initialVelocity, double initialPosition, double delta) {
        if (initialPosition == 0.0 && initialVelocity == 0.0) {
            return 0.0;
        }

        double v0 = (initialPosition < 0) ? -initialVelocity : initialVelocity;
        double p0 = std::abs(initialPosition);

        if (dampingRatio > 1.0) return estimateOverDamped(firstRootReal, secondRootReal, p0, v0, delta);
        if (dampingRatio < 1.0) return estimateUnderDamped(firstRootReal, firstRootImaginary, p0, v0, delta);
        return estimateCriticallyDamped(firstRootReal, p0, v0, delta);
    }

    double estimateAnimationDurationSeconds(double stiffness, double dampingRatio, double initialVelocity,
        double initialDisplacement, double delta) {
        double dampingCoefficient = 2.0 * dampingRatio * std::sqrt(stiffness);

        // Compute the roots of the polynomial [a]x^2+[b]x+[c]=0 which may be complex.
        // Here a is set to the constant 1.0, and folded into the other computations
        double partialRoot = dampingCoefficient * dampingCoefficient - 4.0 * stiffness;
        double partialRootReal = partialRoot < 0.0 ? 0.0 : std::sqrt(partialRoot);
        double partialRootImaginary = partialRoot < 0.0 ? std::sqrt(std::abs(partialRoot)) : 0.0;
        double firstRootReal = (-dampingCoefficient + partialRootReal) * 0.5;
        double firstRootImaginary = partialRootImaginary * 0.5;
        double secondRootReal = (-dampingCoefficient - partialRootReal) * 0.5;

        return estimateDurationSeconds(firstRootReal, firstRootImaginary, secondRootReal, dampingRatio,
            initialVelocity, initialDisplacement, delta);
    }

    // SpringEstimation.estimateAnimationDurationMillis(Float...)
    qint64 estimateAnimationDurationMillis(float stiffness, float dampingRatio, float initialVelocity,
        float initialDisplacement, float delta) {
        if (dampingRatio == 0.f) {
            // No damping, duration is infinite (max value).
            return std::numeric_limits<qint64>::max() / 1000000;
        }
        return static_cast<qint64>(estimateAnimationDurationSeconds(stiffness, dampingRatio, initialVelocity,
                                       initialDisplacement, delta) *
                                   1000.0);
    }

    // FloatSpringSpec(dampingRatio, stiffness, visibilityThreshold).getDurationNanos(0f, 1f, 0f),
    // with ComposeAnimationCoreFlags.isNanosecondAnimationSpecEnabled = false (the default)
    qint64 morphSpringDuration() {
        const float initialValue = 0.f;
        const float targetValue = 1.f;
        const float initialVelocity = 0.f;
        return estimateAnimationDurationMillis(M3LoadingIndicator::MorphSpringStiffness,
            M3LoadingIndicator::MorphSpringDampingRatio,
            initialVelocity / M3LoadingIndicator::MorphSpringVisibilityThreshold,
            (initialValue - targetValue) / M3LoadingIndicator::MorphSpringVisibilityThreshold, 1.f);
    }

    // FloatSpringSpec.getValueFromNanos(playTime (ms-quantised), 0f, 1f, 0f)
    float morphSpringValue(qint64 playTimeMillis) {
        const qint64 playTimeNanos = playTimeMillis * 1000000LL;
        const double deltaTSeconds = static_cast<double>(playTimeNanos) / 1000000000LL;
        return springValue(1.f, M3LoadingIndicator::MorphSpringStiffness, M3LoadingIndicator::MorphSpringDampingRatio,
            0.f, 0.f, deltaTSeconds);
    }

    // ---------------------------------------------------------------------------------------
    // LoadingIndicator.kt
    // ---------------------------------------------------------------------------------------

    /**
     * Builds a list of [Morph]s that describe the sequence of shapes that the [LoadingIndicator]
     * should animate in a loop.
     *
     * This function will create a morph between each consecutive [RoundedPolygon] shaped in the
     * provided array, and will create a final morph from the last shape to the first shape.
     *
     * Note that each [RoundedPolygon] within the returns [Morph]s is normalized here.
     */
    std::vector<Morph> buildMorphSequence(const std::vector<RoundedPolygon>& polygons, bool circularSequence) {
        std::vector<Morph> result;
        for (size_t i = 0; i < polygons.size(); i++) {
            if (i + 1 < polygons.size()) {
                result.emplace_back(polygons[i].normalized(), polygons[i + 1].normalized());
            } else if (circularSequence) {
                // Create a morph from the last shape to the first shape
                result.emplace_back(polygons[i].normalized(), polygons[0].normalized());
            }
        }
        return result;
    }

    /**
     * Calculates a scale factor that will be used when scaling the provided [RoundedPolygon]s
     * into a specified sized container.
     *
     * Since the polygons may rotate, a simple [RoundedPolygon.calculateBounds] is not enough to
     * determine the size the polygon will occupy as it rotates. Using the simple bounds
     * calculation may result in a clipped shape.
     *
     * This function calculates and returns a scale factor by utilizing the
     * [RoundedPolygon.calculateMaxBounds] and comparing its result to the
     * [RoundedPolygon.calculateBounds]. The scale factor can later be used when calling
     * [processPath].
     */
    float calculateScaleFactor(const std::vector<RoundedPolygon>& indicatorPolygons) {
        float scaleFactor = 1.f;
        for (const RoundedPolygon& polygon : indicatorPolygons) {
            auto bounds = polygon.calculateBounds();
            auto maxBounds = polygon.calculateMaxBounds();
            float scaleX = (bounds[2] - bounds[0]) / (maxBounds[2] - maxBounds[0]);
            float scaleY = (bounds[3] - bounds[1]) / (maxBounds[3] - maxBounds[1]);
            // We use max(scaleX, scaleY) to handle cases like a pill-shape that can throw off the
            // entire calculation.
            scaleFactor = std::min(scaleFactor, std::max(scaleX, scaleY));
        }
        return scaleFactor;
    }

    /**
     * Process a given path to scale it and center it inside the given size.
     *
     * @param path a path that was generated by a _normalized_ [Morph] or [RoundedPolygon]
     * @param square the drawing area the path is scaled and centered into
     * @param scaleFactor a scale factor that will be taken into account uniformly when the path
     *   is scaled (i.e. the scaleX would be the width x the scale factor, and the scaleY would be
     *   the height x the scale factor)
     */
    QPainterPath processPath(const QPainterPath& path, const QRectF& square, float scaleFactor) {
        // Scale to the desired size.
        QPainterPath scaled = QTransform::fromScale(square.width() * scaleFactor, square.height() * scaleFactor).map(path);
        // Translate the path to align its center with the available size center.
        // (Compose's Path.getBounds() is the control point bounds.)
        scaled.translate(square.center() - scaled.controlPointRect().center());
        return scaled;
    }

    struct Sequence {
            std::vector<Morph> morphs;
            float scaleFactor;
    };

    const Sequence& indeterminateSequence() {
        static const Sequence sequence = [] {
            const auto& polygons = M3LoadingIndicator::indeterminateIndicatorPolygons();
            // Calculate the shapes scale factor that will be applied to the morphed path as it's
            // scaled into the available size.
            // This overall scale factor ensures that the shapes are rendered without clipping and
            // at the correct ratio within the component by taking into account their occupied
            // size as they rotate, and taking into account the spec's ActiveIndicatorScale.
            return Sequence{buildMorphSequence(polygons, true),
                calculateScaleFactor(polygons) * M3LoadingIndicator::ActiveIndicatorScale};
        }();
        return sequence;
    }

    const Sequence& determinateSequence() {
        static const Sequence sequence = [] {
            const auto& polygons = M3LoadingIndicator::determinateIndicatorPolygons();
            return Sequence{buildMorphSequence(polygons, false),
                calculateScaleFactor(polygons) * M3LoadingIndicator::ActiveIndicatorScale};
        }();
        return sequence;
    }

    // The Spacer: a centred square (aspectRatio(1f, matchHeightConstraintsFirst = true))
    QRectF drawingSquare(const QRectF& bounds) {
        qreal side = std::min(bounds.width(), bounds.height());
        QRectF square(0, 0, side, side);
        square.moveCenter(bounds.center());
        return square;
    }

    QPainterPath rotatedAbout(const QPainterPath& path, const QPointF& pivot, float degrees) {
        // DrawScope.rotate(degrees): clockwise about the centre
        QTransform t;
        t.translate(pivot.x(), pivot.y());
        t.rotate(degrees);
        t.translate(-pivot.x(), -pivot.y());
        return t.map(path);
    }

    // Is every point of [path] (anchors and controls, whose hull contains the curves) inside
    // the convex stadium [bounds]? Then clipping to it would change nothing.
    bool insideStadium(const QPainterPath& path, const QRectF& bounds) {
        const qreal radius = std::min(bounds.width(), bounds.height()) / 2;
        // The stadium is all points within [radius] of its centre segment
        const QPointF c = bounds.center();
        const qreal halfSegX = bounds.width() / 2 - radius;
        const qreal halfSegY = bounds.height() / 2 - radius;
        const qreal limit = radius * radius;
        for (int i = 0; i < path.elementCount(); i++) {
            const QPainterPath::Element e = path.elementAt(i);
            qreal dx = std::max(std::abs(e.x - c.x()) - halfSegX, 0.0);
            qreal dy = std::max(std::abs(e.y - c.y()) - halfSegY, 0.0);
            if (dx * dx + dy * dy > limit) return false;
        }
        return true;
    }

    void paintIndicator(QPainter* painter, const QRectF& bounds, const QPainterPath& indicatorPath, QColor indicator,
        QColor container) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);

        // .clip(containerShape).background(containerColor)
        QPainterPath containerShape = M3LoadingIndicator::containerPath(bounds);
        if (container.isValid() && container.alpha() > 0) {
            painter->fillPath(containerShape, container);
        }

        // Compose clips the indicator to the container shape. The indicator is sized to fit,
        // so only intersect (QPainter clip paths are not antialiased) when it really pokes out.
        if (insideStadium(indicatorPath, bounds)) {
            painter->fillPath(indicatorPath, indicator);
        } else {
            painter->fillPath(indicatorPath.intersected(containerShape), indicator);
        }
        painter->restore();
    }

} // namespace

const std::vector<RoundedPolygon>& M3LoadingIndicator::indeterminateIndicatorPolygons() {
    static const std::vector<RoundedPolygon> polygons = {
        MaterialShapes::softBurst(),
        MaterialShapes::cookie9Sided(),
        MaterialShapes::pentagon(),
        MaterialShapes::pill(),
        MaterialShapes::sunny(),
        MaterialShapes::cookie4Sided(),
        MaterialShapes::oval(),
    };
    return polygons;
}

const std::vector<RoundedPolygon>& M3LoadingIndicator::determinateIndicatorPolygons() {
    static const std::vector<RoundedPolygon> polygons = [] {
        // Rotating the circle gets us a smoother morphing to the soft-burst shapes, which is
        // also being rotated at the same angle.
        // Matrix().apply { rotateZ(360f / 20) }, about the origin as in Compose
        const double r = (360.f / 20) * (3.14159265358979323846 / 180.0);
        const float s = static_cast<float>(std::sin(r));
        const float c = static_cast<float>(std::cos(r));
        RoundedPolygon rotatedCircle = MaterialShapes::circle().transformed([s, c](float x, float y) {
            return m3shapes::TransformResult(c * x + -s * y, s * x + c * y);
        });
        return std::vector<RoundedPolygon>{rotatedCircle, MaterialShapes::softBurst()};
    }();
    return polygons;
}

qint64 M3LoadingIndicator::morphSpringDurationMillis() {
    static const qint64 duration = morphSpringDuration();
    return duration;
}

M3LoadingIndicator::IndeterminateState M3LoadingIndicator::indeterminateState(qint64 elapsedMs) {
    const qint64 elapsed = std::max<qint64>(elapsedMs, 0);
    const qint64 count = static_cast<qint64>(indeterminateSequence().morphs.size());

    // Every MorphIntervalMillis a spring animates morphProgress 0 -> 1. When it finishes (well
    // before the next tick) the morph index advances, morphProgress snaps back to 0 and the
    // rotation target angle advances by a quarter turn.
    const qint64 tick = elapsed / MorphIntervalMillis;
    const qint64 playTime = elapsed % MorphIntervalMillis;
    qint64 morphsFinished;
    float morphProgress;
    if (playTime < morphSpringDurationMillis()) {
        morphsFinished = tick;
        morphProgress = morphSpringValue(playTime);
    } else {
        morphsFinished = tick + 1;
        morphProgress = 0.f;
    }

    // morphRotationTargetAngle starts at QuarterRotation, then
    // (morphRotationTargetAngle + QuarterRotation) % FullRotation after every finished morph
    const float morphRotationTargetAngle = QuarterRotation * static_cast<float>((morphsFinished + 1) % 4);

    // infiniteRepeatable(tween(GlobalRotationDurationMillis, easing = LinearEasing), Restart)
    const float fraction = static_cast<float>(elapsed % GlobalRotationDurationMillis) /
                           static_cast<float>(GlobalRotationDurationMillis);
    const float globalRotation = fraction * FullRotation;

    IndeterminateState state;
    state.morphIndex = static_cast<int>(morphsFinished % count);
    state.morphProgress = morphProgress;
    // Rotate clockwise.
    state.rotation = morphProgress * 90 + morphRotationTargetAngle + globalRotation;
    return state;
}

QPainterPath M3LoadingIndicator::containerPath(const QRectF& bounds) {
    // ShapeKeyTokens.CornerFull: 50% corners
    const qreal radius = std::min(bounds.width(), bounds.height()) / 2;
    QPainterPath path;
    path.addRoundedRect(bounds, radius, radius);
    return path;
}

QPainterPath M3LoadingIndicator::indeterminatePath(const QRectF& bounds, qint64 elapsedMs) {
    const Sequence& sequence = indeterminateSequence();
    const IndeterminateState state = indeterminateState(elapsedMs);
    const QRectF square = drawingSquare(bounds);

    // Use the (uncoerced) spring progress, as upstream.
    QPainterPath path = m3shapes::toPath(sequence.morphs[state.morphIndex], state.morphProgress, 0);
    path = processPath(path, square, sequence.scaleFactor);
    return rotatedAbout(path, square.center(), state.rotation);
}

QPainterPath M3LoadingIndicator::determinatePath(const QRectF& bounds, float progress) {
    const Sequence& sequence = determinateSequence();
    const QRectF square = drawingSquare(bounds);
    const int size = static_cast<int>(sequence.morphs.size());

    // progress().fastCoerceIn(0f, 1f); NaN draws as 0
    const float progressValue = std::isnan(progress) ? 0.f : std::clamp(progress, 0.f, 1.f);
    // Adjust the active morph index according to the progress.
    const int activeMorphIndex = std::min(static_cast<int>(size * progressValue), size - 1);
    // Prepare the progress value that will be used for the active Morph.
    const float adjustedProgressValue = (progressValue == 1.f && activeMorphIndex == size - 1)
                                            // Prevents a zero when the progress is one and we are
                                            // at the last shape morph.
                                            ? 1.f
                                            : std::fmod(progressValue * size, 1.f);

    // Rotate counterclockwise.
    const float rotation = -progressValue * 180;
    QPainterPath path = m3shapes::toPath(sequence.morphs[activeMorphIndex], adjustedProgressValue, 0);
    path = processPath(path, square, sequence.scaleFactor);
    return rotatedAbout(path, square.center(), rotation);
}

void M3LoadingIndicator::paint(QPainter* painter, const QRectF& bounds, qint64 elapsedMs, QColor indicator,
    QColor container) {
    if (bounds.isEmpty()) return;
    paintIndicator(painter, bounds, indeterminatePath(bounds, elapsedMs), indicator, container);
}

void M3LoadingIndicator::paintDeterminate(QPainter* painter, const QRectF& bounds, float progress, QColor indicator,
    QColor container) {
    if (bounds.isEmpty()) return;
    paintIndicator(painter, bounds, determinatePath(bounds, progress), indicator, container);
}

// -------------------------------------------------------------------------------------------

M3LoadingIndicatorWidget::M3LoadingIndicatorWidget(QWidget* parent) :
    QWidget(parent) {
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

M3LoadingIndicatorWidget::~M3LoadingIndicatorWidget() = default;

QColor M3LoadingIndicatorWidget::indicatorColor() const {
    return m_indicatorColor;
}

void M3LoadingIndicatorWidget::setIndicatorColor(const QColor& color) {
    m_indicatorColor = color;
    update();
}

QColor M3LoadingIndicatorWidget::containerColor() const {
    return m_containerColor;
}

void M3LoadingIndicatorWidget::setContainerColor(const QColor& color) {
    m_containerColor = color;
    update();
}

QSize M3LoadingIndicatorWidget::sizeHint() const {
    return QSize(qRound(M3LoadingIndicator::ContainerWidth), qRound(M3LoadingIndicator::ContainerHeight));
}

QSize M3LoadingIndicatorWidget::minimumSizeHint() const {
    return sizeHint();
}

void M3LoadingIndicatorWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    const QColor indicator = m_indicatorColor.isValid() ? m_indicatorColor : palette().color(QPalette::Highlight);
    const qint64 elapsed = m_elapsed.isValid() ? m_elapsed.elapsed() : 0;
    M3LoadingIndicator::paint(&painter, drawingSquare(QRectF(rect())), elapsed, indicator, m_containerColor);
}

void M3LoadingIndicatorWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_elapsed.start();
    m_timer.start();
}

void M3LoadingIndicatorWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    m_timer.stop();
}
