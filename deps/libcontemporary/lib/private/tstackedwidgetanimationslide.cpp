#include "tstackedwidgetanimationslide.h"

#include "tstackedwidget.h"
#include "tvariantanimation.h"
#include <QWidget>
#include "libcontemporary_global.h"
#include <QGraphicsOpacityEffect>

struct tStackedWidgetAnimationSlidePrivate {
        tVariantAnimation* animation;
};

tStackedWidgetAnimationSlide::tStackedWidgetAnimationSlide(int oldIndex, int newIndex, tStackedWidget* parent) :
    tStackedWidgetAnimation{oldIndex, newIndex, parent} {
    d = new tStackedWidgetAnimationSlidePrivate();
}

tStackedWidgetAnimationSlide::~tStackedWidgetAnimationSlide() {
    delete d;
}

void tStackedWidgetAnimationSlide::startAnimation() {
    bool moveLeft;
    if ((this->movingForwards() && !QApplication::isRightToLeft()) || (!this->movingForwards() && QApplication::isRightToLeft())) {
        moveLeft = true;
    } else {
        moveLeft = false;
    }

    this->newWidget()->show();
    this->newWidget()->raise();

    auto* newEffect = new QGraphicsOpacityEffect();
    newEffect->setOpacity(0);
    this->newWidget()->setGraphicsEffect(newEffect);

    //    auto* oldEffect = new QGraphicsOpacityEffect();
    //    oldEffect->setOpacity(1);
    //    this->oldWidget()->setGraphicsEffect(oldEffect);

    // M3 (shared axis): the old page fades out over the first 35% as it
    // moves off, then the new one fades in, so they never show through each
    // other; the M3 "emphasized" curve
    const bool m3 = libContemporaryCommon::isMaterial3();
    QGraphicsOpacityEffect* oldEffect = nullptr;
    if (m3 && this->oldWidget()) {
        oldEffect = new QGraphicsOpacityEffect();
        oldEffect->setOpacity(1);
        this->oldWidget()->setGraphicsEffect(oldEffect);
    }

    auto* animation = new tVariantAnimation(this);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    if (m3) {
        QEasingCurve emphasized(QEasingCurve::BezierSpline);
        emphasized.addCubicBezierSegment(QPointF(0.2, 0), QPointF(0, 1), QPointF(1, 1));
        animation->setEasingCurve(emphasized);
        animation->setDuration(400);
    } else {
        animation->setEasingCurve(QEasingCurve::InOutExpo);
        animation->setDuration(300);
    }
    connect(animation, &tVariantAnimation::valueChanged, this, [this, moveLeft, newEffect, oldEffect, m3](QVariant value) {
        auto progress = value.toReal();
        this->oldWidget()->setGeometry(this->oldWidgetRect(progress));
        this->newWidget()->setGeometry(this->newWidgetRect(progress));
        if (m3) {
            if (oldEffect) oldEffect->setOpacity(qBound(0.0, 1 - progress / 0.35, 1.0));
            newEffect->setOpacity(qBound(0.0, (progress - 0.35) / 0.65, 1.0));
        } else {
            newEffect->setOpacity(progress);
        }
    });
    connect(animation, &tPropertyAnimation::finished, this, [this] {
        this->newWidget()->setGraphicsEffect(nullptr);
        this->oldWidget()->setGraphicsEffect(nullptr);
        this->emitDone();
        this->deleteLater();
    });
    animation->start();
    d->animation = animation;
}

void tStackedWidgetAnimationSlide::stopAnimation() {
    d->animation->stop();
    this->deleteLater();
}
