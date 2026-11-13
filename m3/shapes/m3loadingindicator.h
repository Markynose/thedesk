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
 *            SpringSimulation.kt, SpringEstimation.kt, FloatAnimationSpec.kt (Copyright 2019/2020)
 *
 * The coroutine-driven Compose animation is a pure function of elapsed time
 * here, so anything with a timer can draw the indicator.
 */
#ifndef M3LOADINGINDICATOR_H
#define M3LOADINGINDICATOR_H

#include "morph.h"
#include "roundedpolygon.h"

#include <QColor>
#include <QElapsedTimer>
#include <QRectF>
#include <QTimer>
#include <QWidget>
#include <vector>

class QPainter;

/**
 * Material 3 Expressive loading indicator: the active indicator morphs between a sequence of
 * MaterialShapes while rotating, optionally inside a fully rounded container.
 */
class M3LoadingIndicator {
    public:
        // ---- LoadingIndicatorTokens (v0_7_0) ----
        static constexpr qreal ContainerWidth = 48.0;  // dp
        static constexpr qreal ContainerHeight = 48.0; // dp
        static constexpr qreal ActiveSize = 38.0;      // dp; LoadingIndicatorDefaults.IndicatorSize
        // ContainerShape = ShapeKeyTokens.CornerFull (a stadium / circle)
        // ActiveIndicatorColor = Primary; ContainedActiveColor = OnPrimaryContainer;
        // ContainedContainerColor = PrimaryContainer

        // ---- LoadingIndicator.kt ----
        static constexpr int GlobalRotationDurationMillis = 4666; // linear 0..360 deg, restarting
        static constexpr int MorphIntervalMillis = 650;           // one morph starts every 650 ms
        static constexpr float FullRotation = 360.f;
        static constexpr float QuarterRotation = FullRotation / 4.f;
        // spring(dampingRatio = 0.6f, stiffness = 200f, visibilityThreshold = 0.1f)
        static constexpr float MorphSpringDampingRatio = 0.6f;
        static constexpr float MorphSpringStiffness = 200.f;
        static constexpr float MorphSpringVisibilityThreshold = 0.1f;

        // ActiveIndicatorScale = IndicatorSize / min(ContainerWidth, ContainerHeight)
        static constexpr float ActiveIndicatorScale = static_cast<float>(ActiveSize / (ContainerWidth < ContainerHeight ? ContainerWidth : ContainerHeight));

        // LoadingIndicatorDefaults.IndeterminateIndicatorPolygons: SoftBurst, Cookie9Sided,
        // Pentagon, Pill, Sunny, Cookie4Sided, Oval
        static const std::vector<m3shapes::RoundedPolygon>& indeterminateIndicatorPolygons();
        // LoadingIndicatorDefaults.DeterminateIndicatorPolygons: Circle rotated 18 deg, SoftBurst
        static const std::vector<m3shapes::RoundedPolygon>& determinateIndicatorPolygons();

        // Duration of each morph spring as Compose estimates it (visibility threshold 0.1):
        // 297 ms; the indicator then holds the reached shape until the next 650 ms tick.
        static qint64 morphSpringDurationMillis();

        // The indeterminate animation state at a point in time
        struct IndeterminateState {
                int morphIndex;       // into the circular morph sequence (7 morphs)
                float morphProgress;  // spring value; overshoots 1 (not coerced, as upstream)
                float rotation;       // degrees, clockwise
        };
        static IndeterminateState indeterminateState(qint64 elapsedMs);

        // The indicator's path for the given state, already scaled, centred and rotated in
        // [bounds] (the container's rect), as drawn by Compose.
        static QPainterPath indeterminatePath(const QRectF& bounds, qint64 elapsedMs);
        static QPainterPath determinatePath(const QRectF& bounds, float progress);

        // The container: bounds with fully rounded corners (CornerFull)
        static QPainterPath containerPath(const QRectF& bounds);

        /**
         * Draws the indeterminate indicator at [elapsedMs] since it appeared.
         * @param bounds the container (48x48 dp by default); the indicator is drawn in the
         *   centred square inside it at 38/48 of its size
         * @param indicator the indicator colour (Primary; OnPrimaryContainer when contained)
         * @param container the container colour (PrimaryContainer), or invalid to draw the
         *   uncontained indicator
         */
        static void paint(QPainter* painter, const QRectF& bounds, qint64 elapsedMs, QColor indicator,
            QColor container = QColor());

        // The determinate indicator: morphs Circle -> SoftBurst as progress goes 0..1,
        // rotating counterclockwise by 180 degrees.
        static void paintDeterminate(QPainter* painter, const QRectF& bounds, float progress, QColor indicator,
            QColor container = QColor());
};

/**
 * A QWidget showing the indeterminate M3 loading indicator. The animation runs only while the
 * widget is visible, restarting each time it is shown (as the composable restarts when it enters
 * the composition).
 */
class M3LoadingIndicatorWidget : public QWidget {
        Q_OBJECT
        Q_PROPERTY(QColor indicatorColor READ indicatorColor WRITE setIndicatorColor)
        Q_PROPERTY(QColor containerColor READ containerColor WRITE setContainerColor)

    public:
        explicit M3LoadingIndicatorWidget(QWidget* parent = nullptr);
        ~M3LoadingIndicatorWidget() override;

        // Invalid (the default) = palette().color(QPalette::Highlight)
        QColor indicatorColor() const;
        void setIndicatorColor(const QColor& color);

        // Invalid (the default) = not contained
        QColor containerColor() const;
        void setContainerColor(const QColor& color);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        QTimer m_timer;
        QElapsedTimer m_elapsed;
        QColor m_indicatorColor;
        QColor m_containerColor;
};

#endif // M3LOADINGINDICATOR_H
