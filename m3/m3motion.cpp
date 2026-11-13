/****************************************
 *
 *   theDesk - Material 3 Expressive motion
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The spring maths (step()) is ported from Jetpack Compose's
 *   SpringSimulation.updateValuesSeconds (androidx, Copyright 2019 The
 *   Android Open Source Project, Apache License 2.0); modified for C++/Qt.
 *
 * *************************************/
#include "m3motion.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>
#include <cmath>
#include <tsettings.h>

namespace {
    struct Motion {
            double value;
            double velocity;
    };

    // Compose SpringSimulation.updateValuesSeconds, for one component
    Motion step(double position, double velocity, double target, M3Motion::Spring spring, double dt) {
        const double naturalFreq = std::sqrt(spring.stiffness);
        const double dampingRatio = spring.dampingRatio;
        const double adjustedDisplacement = position - target;
        const double dampingRatioSquared = dampingRatio * dampingRatio;
        const double r = -dampingRatio * naturalFreq;

        double displacement;
        double currentVelocity;
        if (dampingRatio > 1) {
            // Overdamped
            double s = naturalFreq * std::sqrt(dampingRatioSquared - 1);
            double gammaPlus = r + s;
            double gammaMinus = r - s;
            double coeffB = (gammaMinus * adjustedDisplacement - velocity) / (gammaMinus - gammaPlus);
            double coeffA = adjustedDisplacement - coeffB;
            displacement = coeffA * std::exp(gammaMinus * dt) + coeffB * std::exp(gammaPlus * dt);
            currentVelocity = coeffA * gammaMinus * std::exp(gammaMinus * dt) + coeffB * gammaPlus * std::exp(gammaPlus * dt);
        } else if (dampingRatio == 1) {
            // Critically damped
            double coeffA = adjustedDisplacement;
            double coeffB = velocity + naturalFreq * adjustedDisplacement;
            double nFdT = -naturalFreq * dt;
            displacement = (coeffA + coeffB * dt) * std::exp(nFdT);
            currentVelocity = ((coeffA + coeffB * dt) * std::exp(nFdT) * (-naturalFreq)) + coeffB * std::exp(nFdT);
        } else {
            // Underdamped: bounces
            double dampedFreq = naturalFreq * std::sqrt(1 - dampingRatioSquared);
            double cosCoeff = adjustedDisplacement;
            double sinCoeff = (1 / dampedFreq) * ((-r * adjustedDisplacement) + velocity);
            double dFdT = dampedFreq * dt;
            displacement = std::exp(r * dt) * (cosCoeff * std::cos(dFdT) + sinCoeff * std::sin(dFdT));
            currentVelocity = displacement * r + std::exp(r * dt) * (-dampedFreq * cosCoeff * std::sin(dFdT) + dampedFreq * sinCoeff * std::cos(dFdT));
        }
        return {displacement + target, currentVelocity};
    }

    // Normalised settling: within 0.1% of the distance and nearly still
    constexpr double settleDisplacement = 0.001;
    constexpr double settleVelocity = 0.01;

    int settleMs(M3Motion::Spring spring) {
        Motion m{0, 0};
        for (int ms = 1; ms < 10000; ms++) {
            m = step(m.value, m.velocity, 1, spring, 0.001);
            if (std::abs(m.value - 1) < settleDisplacement && std::abs(m.velocity) < settleVelocity) return ms;
        }
        return 10000;
    }

    // Easing curves need plain function pointers, so one per token/scheme
    template<int token, bool expressive> qreal springEasing(qreal progress) {
        static const M3Motion::Spring s = M3Motion::spring(M3Motion::Token(token), expressive);
        static const double length = settleMs(s) / 1000.0;
        if (progress >= 1) return 1;
        return step(0, 0, 1, s, progress * length).value;
    }

    using EasingFunction = qreal (*)(qreal);
    template<bool expressive> EasingFunction easingFor(M3Motion::Token token) {
        switch (token) {
            case M3Motion::SpatialFast:
                return springEasing<M3Motion::SpatialFast, expressive>;
            case M3Motion::SpatialDefault:
                return springEasing<M3Motion::SpatialDefault, expressive>;
            case M3Motion::SpatialSlow:
                return springEasing<M3Motion::SpatialSlow, expressive>;
            case M3Motion::EffectsFast:
                return springEasing<M3Motion::EffectsFast, expressive>;
            case M3Motion::EffectsDefault:
                return springEasing<M3Motion::EffectsDefault, expressive>;
            case M3Motion::EffectsSlow:
            default:
                return springEasing<M3Motion::EffectsSlow, expressive>;
        }
    }

    // QVariant <-> component vector
    std::vector<double> components(const QVariant& value) {
        switch (value.typeId()) {
            case QMetaType::QPoint:
            case QMetaType::QPointF: {
                QPointF p = value.toPointF();
                return {p.x(), p.y()};
            }
            case QMetaType::QSize:
            case QMetaType::QSizeF: {
                QSizeF s = value.toSizeF();
                return {s.width(), s.height()};
            }
            case QMetaType::QRect:
            case QMetaType::QRectF: {
                QRectF r = value.toRectF();
                return {r.x(), r.y(), r.width(), r.height()};
            }
            case QMetaType::QColor: {
                QColor c = value.value<QColor>().toRgb();
                return {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
            }
            default:
                return {value.toDouble()};
        }
    }

    QVariant fromComponents(const std::vector<double>& c, int typeId) {
        switch (typeId) {
            case QMetaType::Int:
                return int(std::lround(c[0]));
            case QMetaType::QPoint:
                return QPointF(c[0], c[1]).toPoint();
            case QMetaType::QPointF:
                return QPointF(c[0], c[1]);
            case QMetaType::QSize:
                return QSizeF(c[0], c[1]).toSize();
            case QMetaType::QSizeF:
                return QSizeF(c[0], c[1]);
            case QMetaType::QRect:
                return QRectF(c[0], c[1], c[2], c[3]).toRect();
            case QMetaType::QRectF:
                return QRectF(c[0], c[1], c[2], c[3]);
            case QMetaType::QColor:
                return QColor::fromRgbF(qBound(0.0, c[0], 1.0), qBound(0.0, c[1], 1.0), qBound(0.0, c[2], 1.0), qBound(0.0, c[3], 1.0));
            default:
                return c[0];
        }
    }

    // Absolute settle threshold for a component, Compose-style: half a pixel
    // for geometry, a fraction of a step for colours and plain numbers
    double threshold(int typeId, double distance) {
        switch (typeId) {
            case QMetaType::Int:
            case QMetaType::QPoint:
            case QMetaType::QSize:
            case QMetaType::QRect:
            case QMetaType::QPointF:
            case QMetaType::QSizeF:
            case QMetaType::QRectF:
                return 0.5;
            case QMetaType::QColor:
                return 0.5 / 255;
            default:
                return qMax(1e-4, std::abs(distance) * settleDisplacement);
        }
    }
} // namespace

bool M3Motion::expressive() {
    // Read once per process; changing the scheme applies to newly started apps
    static const bool isExpressive = [] {
        tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
        return settings.value(QStringLiteral("Platform/motion")).toString() != QStringLiteral("standard");
    }();
    return isExpressive;
}

M3Motion::SpringState M3Motion::stepSpring(double value, double velocity, double target, Spring spring, double dtSeconds) {
    Motion m{value, velocity};
    // Same 16 ms slicing as M3SpringAnimation
    while (dtSeconds > 0) {
        double dt = qMin(dtSeconds, 0.016);
        dtSeconds -= dt;
        m = step(m.value, m.velocity, target, spring, dt);
    }
    return {m.value, m.velocity};
}

M3Motion::Spring M3Motion::spring(Token token) {
    return spring(token, expressive());
}

M3Motion::Spring M3Motion::spring(Token token, bool expressive) {
    // Compose tokens/ExpressiveMotionTokens.kt and StandardMotionTokens.kt
    switch (token) {
        case SpatialFast:
            return expressive ? Spring{0.6, 800} : Spring{0.9, 1400};
        case SpatialDefault:
            return expressive ? Spring{0.8, 380} : Spring{0.9, 700};
        case SpatialSlow:
            return expressive ? Spring{0.8, 200} : Spring{0.9, 300};
        case EffectsFast:
            return {1.0, 3800};
        case EffectsDefault:
            return {1.0, 1600};
        case EffectsSlow:
        default:
            return {1.0, 800};
    }
}

QEasingCurve M3Motion::easing(Token token) {
    QEasingCurve curve;
    curve.setCustomType(expressive() ? easingFor<true>(token) : easingFor<false>(token));
    return curve;
}

int M3Motion::duration(Token token) {
    return settleMs(spring(token));
}

struct M3SpringAnimation::Private {
        QPointer<QObject> target;
        QByteArray propertyName;
        M3Motion::Spring spring = M3Motion::spring(M3Motion::SpatialDefault);

        int typeId = QMetaType::Double;
        QVariant startValue;
        QVariant endValue;
        std::vector<double> position;
        std::vector<double> velocity;
        std::vector<double> targetPosition;
        std::vector<double> thresholds;
        int lastTime = 0;
};

M3SpringAnimation::M3SpringAnimation(QObject* parent) :
    QAbstractAnimation(parent) {
    d = new Private();
}

M3SpringAnimation::M3SpringAnimation(QObject* target, const QByteArray& propertyName, QObject* parent) :
    M3SpringAnimation(parent) {
    d->target = target;
    d->propertyName = propertyName;
}

M3SpringAnimation::~M3SpringAnimation() {
    delete d;
}

void M3SpringAnimation::setToken(M3Motion::Token token) {
    d->spring = M3Motion::spring(token);
}

void M3SpringAnimation::setSpring(M3Motion::Spring spring) {
    d->spring = spring;
}

void M3SpringAnimation::setStartValue(const QVariant& value) {
    d->startValue = value;
    d->typeId = value.typeId();
    d->position = components(value);
    d->velocity.assign(d->position.size(), 0);
    if (d->targetPosition.size() != d->position.size()) d->targetPosition = d->position;
}

void M3SpringAnimation::setEndValue(const QVariant& value) {
    d->endValue = value;
    d->typeId = value.typeId();
    d->targetPosition = components(value);
    if (d->position.size() != d->targetPosition.size()) {
        // No usable start yet: start from the target's shape at rest
        d->position = d->targetPosition;
        d->velocity.assign(d->position.size(), 0);
    }
    d->thresholds.resize(d->targetPosition.size());
    for (size_t i = 0; i < d->targetPosition.size(); i++) {
        d->thresholds[i] = threshold(d->typeId, d->targetPosition[i] - d->position[i]);
    }
}

QVariant M3SpringAnimation::startValue() const {
    return d->startValue;
}

QVariant M3SpringAnimation::endValue() const {
    return d->endValue;
}

QVariant M3SpringAnimation::currentValue() const {
    if (d->position.empty()) return d->endValue;
    return fromComponents(d->position, d->typeId);
}

void M3SpringAnimation::animateTo(const QVariant& value) {
    if (d->position.empty()) setStartValue(value);
    setEndValue(value);
    if (state() != Running) start();
}

void M3SpringAnimation::snapTo(const QVariant& value) {
    stop();
    setStartValue(value);
    setEndValue(value);
    writeValue();
}

int M3SpringAnimation::duration() const {
    return -1;
}

void M3SpringAnimation::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    if (newState == Running && oldState == Stopped) d->lastTime = 0;
    QAbstractAnimation::updateState(newState, oldState);
}

void M3SpringAnimation::updateCurrentTime(int currentTime) {
    // Step in at most 16 ms slices so a stalled frame can't explode the maths
    double elapsed = qMax(0, currentTime - d->lastTime) / 1000.0;
    d->lastTime = currentTime;
    if (d->position.size() != d->targetPosition.size()) return;

    bool settled = true;
    while (elapsed > 0) {
        double dt = qMin(elapsed, 0.016);
        elapsed -= dt;
        for (size_t i = 0; i < d->position.size(); i++) {
            auto m = step(d->position[i], d->velocity[i], d->targetPosition[i], d->spring, dt);
            d->position[i] = m.value;
            d->velocity[i] = m.velocity;
        }
    }
    for (size_t i = 0; i < d->position.size(); i++) {
        double thr = i < d->thresholds.size() ? d->thresholds[i] : 1e-4;
        if (std::abs(d->position[i] - d->targetPosition[i]) >= thr || std::abs(d->velocity[i]) >= thr * 60) settled = false;
    }

    if (settled) {
        d->position = d->targetPosition;
        d->velocity.assign(d->position.size(), 0);
        writeValue();
        stop(); // QAbstractAnimation emits finished()
        return;
    }
    writeValue();
}

void M3SpringAnimation::writeValue() {
    QVariant value = currentValue();
    if (d->target && !d->propertyName.isEmpty()) d->target->setProperty(d->propertyName.constData(), value);
    emit valueChanged(value);
}
