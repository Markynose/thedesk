/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Ripple timing follows Compose's RippleAnimation (androidx, Apache 2.0):
 *   75 ms fade in, 225 ms radius (FastOutSlowIn), 150 ms fade out.
 *
 * *************************************/
#include "m3animator.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QEasingCurve>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>
#include <QtMath>
#include <QApplication>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <m3cursor.h>
#include <m3motion.h>

namespace {
    constexpr int rippleFadeInMs = 75;
    constexpr int rippleRadiusMs = 225;
    constexpr int rippleFadeOutMs = 150;
    constexpr double pressedAlpha = 0.10; // M3 StateTokens.PressedStateLayerOpacity

    QElapsedTimer& styleClock() {
        static QElapsedTimer timer = [] {
            QElapsedTimer t;
            t.start();
            return t;
        }();
        return timer;
    }

    double fastOutSlowIn(double t) {
        static const QEasingCurve curve = [] {
            QEasingCurve c(QEasingCurve::BezierSpline);
            c.addCubicBezierSegment(QPointF(0.4, 0), QPointF(0.2, 1), QPointF(1, 1));
            return c;
        }();
        return curve.valueForProgress(qBound(0.0, t, 1.0));
    }
} // namespace

M3Animator::M3Animator(QObject* parent) :
    QObject(parent) {
    frameTimer.setInterval(16);
    connect(&frameTimer, &QTimer::timeout, this, &M3Animator::tick);
    cursorWatch.setInterval(120);
    connect(&cursorWatch, &QTimer::timeout, this, &M3Animator::checkCursorOwner);
}

M3Animator::~M3Animator() {
    if (cursorOverridden && qApp) QGuiApplication::restoreOverrideCursor();
}

void M3Animator::setCursorMergePredicate(std::function<bool(const QWidget*)> predicate) {
    cursorMergeAllowed = std::move(predicate);
}

void M3Animator::cursorEnter(QWidget* widget) {
    if (!M3Cursor::active() || !cursorMergeAllowed || !cursorMergeAllowed(widget)) return;
    if (!cursorScale) {
        cursorScale = new M3SpringAnimation(this);
        cursorScale->setToken(M3Motion::SpatialFast);
        cursorScale->setStartValue(1.0);
        connect(cursorScale, &M3SpringAnimation::valueChanged, this, [this](QVariant value) {
            applyCursorScale(value.toDouble());
        });
        connect(cursorScale, &QAbstractAnimation::stateChanged, this, [this](QAbstractAnimation::State state) {
            // Grown back and settled: hand the pointer back to the app
            if (state == QAbstractAnimation::Stopped && !cursorOwner && cursorOverridden) {
                QGuiApplication::restoreOverrideCursor();
                cursorOverridden = false;
            }
        });
    }
    cursorOwner = widget;
    if (!cursorOverridden) {
        QGuiApplication::setOverrideCursor(Qt::ArrowCursor);
        cursorOverridden = true;
    }
    cursorScale->animateTo(0.0);
    cursorWatch.start();
}

void M3Animator::cursorLeave() {
    cursorOwner = nullptr;
    cursorWatch.stop();
    if (cursorScale && cursorOverridden) cursorScale->animateTo(1.0);
}

void M3Animator::checkCursorOwner() {
    // Leave isn't always sent (a popup or dialog opened, the window hid)
    QWidget* owner = cursorOwner;
    if (!owner || !owner->isVisible() || !owner->isEnabled()) return cursorLeave();
    QWidget* under = QApplication::widgetAt(QCursor::pos());
    if (under != owner && !owner->isAncestorOf(under)) cursorLeave();
}

void M3Animator::applyCursorScale(double scale) {
    if (!cursorOverridden) return;
    if (scale <= 0.04) {
        QGuiApplication::changeOverrideCursor(Qt::BlankCursor);
        return;
    }
    // Quantised so the springs reuse a handful of cursors (overshoot included)
    int step = qBound(1, qRound(scale * 20), 24);
    auto it = cursorCache.constFind(step);
    if (it == cursorCache.constEnd()) {
        QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
        double dpr = screen ? screen->devicePixelRatio() : qApp->devicePixelRatio();
        int size = qRound(M3Cursor::nominalSize() * dpr);
        // A little canvas room for the overshoot past full size
        int canvas = qRound(size * 1.25);
        QImage image(canvas, canvas, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QImage arrow = M3Cursor::renderArrow(canvas, M3Cursor::schemeColours(), step / 20.0 * size / double(canvas));
            QPainter p(&image);
            p.drawImage(0, 0, arrow);
        }
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(dpr);
        QPointF tip = M3Cursor::arrowHotspot() * canvas / M3Cursor::DesignSize / dpr;
        it = cursorCache.insert(step, QCursor(pixmap, qRound(tip.x()), qRound(tip.y())));
    }
    QGuiApplication::changeOverrideCursor(*it);
}

qint64 M3Animator::now() {
    return styleClock().elapsed();
}

void M3Animator::watch(QWidget* widget) {
    if (!widget || states.contains(widget)) return;
    states.insert(widget, State());
    widget->installEventFilter(this);
    connect(widget, &QObject::destroyed, this, [this, widget] {
        states.remove(widget);
    });

    if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
        connect(button, &QAbstractButton::toggled, this, [this, button](bool on) {
            auto it = states.find(button);
            if (it == states.end()) return;
            setTarget(button, it->checked, M3Motion::SpatialFast, on ? 1 : 0, on ? 0 : 1);
        });
    }
    if (auto* slider = qobject_cast<QAbstractSlider*>(widget)) {
        connect(slider, &QAbstractSlider::sliderPressed, this, [this, slider] {
            auto it = states.find(slider);
            if (it != states.end()) setTarget(slider, it->press, M3Motion::SpatialFast, 1, 0);
        });
        connect(slider, &QAbstractSlider::sliderReleased, this, [this, slider] {
            auto it = states.find(slider);
            if (it != states.end()) setTarget(slider, it->press, M3Motion::SpatialFast, 0, 1);
        });
    }
}

void M3Animator::unwatch(QWidget* widget) {
    if (!widget) return;
    widget->removeEventFilter(this);
    disconnect(widget, nullptr, this, nullptr);
    auto it = states.find(widget);
    if (it != states.end()) {
        delete it->hover;
        delete it->press;
        delete it->checked;
        delete it->merge;
        states.erase(it);
    }
    busy.removeAll(QPointer<QWidget>(widget));
}

void M3Animator::watchBusy(QWidget* widget) {
    if (!busy.contains(QPointer<QWidget>(widget))) busy.append(QPointer<QWidget>(widget));
    if (!frameTimer.isActive()) frameTimer.start();
}

M3SpringAnimation* M3Animator::spring(QWidget* widget, M3SpringAnimation*& slot, int token, double initial) {
    if (!slot) {
        slot = new M3SpringAnimation(this);
        slot->setToken(M3Motion::Token(token));
        slot->setStartValue(initial);
        slot->setEndValue(initial);
        QPointer<QWidget> guard(widget);
        connect(slot, &M3SpringAnimation::valueChanged, this, [guard] {
            if (guard) guard->update();
        });
    }
    return slot;
}

void M3Animator::setTarget(QWidget* widget, M3SpringAnimation*& slot, int token, double target, double initial) {
    spring(widget, slot, token, initial)->animateTo(target);
}

double M3Animator::hover(const QWidget* widget, bool fallback) const {
    auto it = states.constFind(widget);
    if (it == states.constEnd() || !it->hover) return fallback ? 1 : 0;
    return qBound(0.0, it->hover->currentValue().toDouble(), 1.0);
}

double M3Animator::press(const QWidget* widget, bool fallback) const {
    auto it = states.constFind(widget);
    if (it == states.constEnd() || !it->press) return fallback ? 1 : 0;
    return it->press->currentValue().toDouble(); // may overshoot: it's a shape
}

double M3Animator::checked(const QWidget* widget, bool fallback) const {
    auto it = states.constFind(widget);
    if (it == states.constEnd() || !it->checked) return fallback ? 1 : 0;
    return it->checked->currentValue().toDouble();
}

void M3Animator::startMerge(QWidget* widget, State& state, QPointF entry) {
    // Re-entering while the old fill is still fading keeps it; otherwise a new
    // fill starts from the entry point
    double visible = state.hover ? state.hover->currentValue().toDouble() : 0;
    auto* merge = spring(widget, state.merge, M3Motion::SpatialFast, 0);
    if (visible < 0.05) {
        state.entry = entry;
        merge->snapTo(0.0);
    }
    merge->animateTo(1.0);
}

QPainterPath M3Animator::mergePath(const QWidget* widget, const QPainterPath& shape, double* opacity) const {
    auto it = states.constFind(widget);
    if (it == states.constEnd() || !it->merge || !it->hover) return {};
    double visible = qBound(0.0, it->hover->currentValue().toDouble(), 1.0);
    double coverage = it->merge->currentValue().toDouble();
    if (visible <= 0.001 || coverage <= 0.001) return {};

    const QRectF b = shape.boundingRect();
    const QPointF o = b.contains(it->entry) ? it->entry : QPointF(qBound(b.left(), it->entry.x(), b.right()), qBound(b.top(), it->entry.y(), b.bottom()));
    double reach = 0;
    for (QPointF corner : {b.topLeft(), b.topRight(), b.bottomLeft(), b.bottomRight()}) reach = qMax(reach, QLineF(o, corner).length());
    double radius = qMax(0.0, coverage) * (reach + 1);

    QPainterPath circle;
    circle.addEllipse(o, radius, radius);
    if (opacity) *opacity = visible;
    return coverage >= 1 ? shape : shape.intersected(circle);
}

void M3Animator::startRipple(QWidget* widget, QPointF origin) {
    auto it = states.find(widget);
    if (it == states.end()) return;
    Ripple ripple;
    ripple.origin = origin;
    ripple.start = now();
    it->ripples.append(ripple);
    if (!frameTimer.isActive()) frameTimer.start();
}

void M3Animator::releaseRipples(QWidget* widget) {
    auto it = states.find(widget);
    if (it == states.end()) return;
    for (auto& ripple : it->ripples) {
        if (ripple.release < 0) ripple.release = now();
    }
}

void M3Animator::paintRipples(QPainter* painter, const QWidget* widget, const QPainterPath& shape, QColor color) const {
    auto it = states.constFind(widget);
    if (it == states.constEnd() || it->ripples.isEmpty()) return;

    const QRectF bounds = shape.boundingRect();
    const QPointF centre = bounds.center();
    // Compose: start at 30% of the larger side, end past the farthest corner
    const double startRadius = qMax(bounds.width(), bounds.height()) * 0.3;
    const double endRadius = std::hypot(bounds.width(), bounds.height()) / 2 + 10;
    const qint64 t = now();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipPath(shape, Qt::IntersectClip);
    painter->setPen(Qt::NoPen);
    for (const Ripple& ripple : it->ripples) {
        double elapsed = t - ripple.start;
        double alpha = qMin(1.0, elapsed / rippleFadeInMs);
        if (ripple.release >= 0) {
            // Fade out once released, but never before the fade-in finished
            double fadeStart = qMax<double>(ripple.release, ripple.start + rippleFadeInMs);
            alpha *= 1 - qBound(0.0, (t - fadeStart) / rippleFadeOutMs, 1.0);
        }
        double progress = fastOutSlowIn(elapsed / rippleRadiusMs);
        double radius = startRadius + (endRadius - startRadius) * progress;
        QPointF origin = ripple.origin + (centre - ripple.origin) * qMin(1.0, elapsed / rippleRadiusMs);

        QColor c = color;
        c.setAlphaF(color.alphaF() * pressedAlpha * alpha);
        painter->setBrush(c);
        painter->drawEllipse(origin, radius, radius);
    }
    painter->restore();
}

void M3Animator::tick() {
    const qint64 t = now();
    bool active = false;
    for (auto it = states.begin(); it != states.end(); it++) {
        if (it->ripples.isEmpty()) continue;
        it->ripples.erase(std::remove_if(it->ripples.begin(), it->ripples.end(), [t](const Ripple& r) {
            return r.release >= 0 && t - qMax<qint64>(r.release, r.start + rippleFadeInMs) > rippleFadeOutMs;
        }),
            it->ripples.end());
        const_cast<QWidget*>(it.key())->update();
        if (!it->ripples.isEmpty()) active = true;
    }
    for (auto it = busy.begin(); it != busy.end();) {
        if (it->isNull()) {
            it = busy.erase(it);
            continue;
        }
        if ((*it)->isVisible()) {
            (*it)->update();
            active = true;
        }
        it++;
    }
    if (!active && busy.isEmpty()) frameTimer.stop();
}

bool M3Animator::eventFilter(QObject* watched, QEvent* event) {
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget) return false;
    auto it = states.find(widget);
    if (it == states.end()) return false;

    switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            if (widget->isEnabled()) {
                startMerge(widget, *it, static_cast<QSinglePointEvent*>(event)->position());
                if (cursorOwner != widget) cursorEnter(widget);
            }
            setTarget(widget, it->hover, M3Motion::EffectsFast, 1, 0);
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
        case QEvent::Hide:
            if (event->type() != QEvent::Hide) setTarget(widget, it->hover, M3Motion::EffectsFast, 0, 1);
            if (cursorOwner == widget) cursorLeave();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton && widget->isEnabled()) {
                startRipple(widget, static_cast<QMouseEvent*>(event)->position());
                if (!qobject_cast<QAbstractSlider*>(widget)) setTarget(widget, it->press, M3Motion::SpatialFast, 1, 0);
            }
            break;
        case QEvent::MouseButtonRelease:
            releaseRipples(widget);
            if (!qobject_cast<QAbstractSlider*>(widget)) setTarget(widget, it->press, M3Motion::SpatialFast, 0, 1);
            break;
        case QEvent::KeyPress: {
            auto* key = static_cast<QKeyEvent*>(event);
            if (!key->isAutoRepeat() && (key->key() == Qt::Key_Space) && qobject_cast<QAbstractButton*>(widget)) {
                startRipple(widget, QRectF(widget->rect()).center());
                setTarget(widget, it->press, M3Motion::SpatialFast, 1, 0);
            }
            break;
        }
        case QEvent::KeyRelease: {
            auto* key = static_cast<QKeyEvent*>(event);
            if (!key->isAutoRepeat() && key->key() == Qt::Key_Space && qobject_cast<QAbstractButton*>(widget)) {
                releaseRipples(widget);
                setTarget(widget, it->press, M3Motion::SpatialFast, 0, 1);
            }
            break;
        }
        default:
            break;
    }
    return false;
}
