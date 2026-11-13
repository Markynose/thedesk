/****************************************
 *
 *   theDesk - Material 3 Expressive motion
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The spring maths is ported from Jetpack Compose's SpringSimulation
 *   (androidx, Copyright 2019 The Android Open Source Project, Apache 2.0);
 *   the spring tokens are Compose's ExpressiveMotionTokens and
 *   StandardMotionTokens (https://m3.material.io/styles/motion/overview).
 *
 * *************************************/
#ifndef M3MOTION_H
#define M3MOTION_H

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QPointer>
#include <QVariant>
#include <vector>

namespace M3Motion {
    // MotionScheme tokens: "spatial" moves things (position, size, shape) and
    // may bounce; "effects" changes colour/opacity and never overshoots.
    enum Token {
        SpatialFast,
        SpatialDefault,
        SpatialSlow,
        EffectsFast,
        EffectsDefault,
        EffectsSlow
    };

    struct Spring {
            double dampingRatio;
            double stiffness;
    };

    // Expressive by default; Platform/motion=standard in
    // theSuite/theDesk.platform picks the calmer standard scheme.
    bool expressive();
    Spring spring(Token token);
    Spring spring(Token token, bool expressive);

    // For code that needs a fixed-duration QVariantAnimation: the spring's
    // motion from rest as an easing curve, and how long it takes to settle.
    QEasingCurve easing(Token token);
    int duration(Token token);

    // One spring step (Compose SpringSimulation), for code that drives many
    // springs from a single frame timer
    struct SpringState {
            double value;
            double velocity;
    };
    SpringState stepSpring(double value, double velocity, double target, Spring spring, double dtSeconds);
} // namespace M3Motion

/**
 * A spring animation in the style of Compose's animate*AsState: no fixed
 * duration, it runs until the value settles. Calling animateTo() while it is
 * running retargets it and keeps the current velocity, so interrupted
 * motion carries its momentum.
 *
 * Animates int, double, QPoint(F), QSize(F), QRect(F) and QColor values.
 * With a target object and property name it writes that property too.
 */
class M3SpringAnimation : public QAbstractAnimation {
        Q_OBJECT
    public:
        explicit M3SpringAnimation(QObject* parent = nullptr);
        M3SpringAnimation(QObject* target, const QByteArray& propertyName, QObject* parent = nullptr);
        ~M3SpringAnimation() override;

        void setToken(M3Motion::Token token);
        void setSpring(M3Motion::Spring spring);

        void setStartValue(const QVariant& value); // jumps there, velocity 0
        void setEndValue(const QVariant& value);   // target; keeps velocity
        QVariant startValue() const;
        QVariant endValue() const;
        QVariant currentValue() const;

        // Retarget (or start) towards `value` from wherever it is now
        void animateTo(const QVariant& value);
        // Jump straight to `value` and stop
        void snapTo(const QVariant& value);

        int duration() const override; // -1: runs until settled

    signals:
        void valueChanged(QVariant value);

    protected:
        void updateCurrentTime(int currentTime) override;
        void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) override;

    private:
        struct Private;
        Private* d;

        void writeValue();
};

#endif // M3MOTION_H
