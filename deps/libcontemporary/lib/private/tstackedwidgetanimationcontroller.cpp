#include "tstackedwidgetanimationcontroller.h"

#include "tstackedwidgetanimation.h"
#include <QPointer>

struct tStackedWidgetAnimationControllerPrivate {
        tStackedWidget* parentStack;
        QPointer<tStackedWidgetAnimation> currentAnimation;
};

tStackedWidgetAnimationController::tStackedWidgetAnimationController(tStackedWidget* parentStack, QObject* parent) :
    QObject{parent} {
    d = new tStackedWidgetAnimationControllerPrivate();
    d->parentStack = parentStack;
}

tStackedWidgetAnimationController::~tStackedWidgetAnimationController() {
    delete d;
}

void tStackedWidgetAnimationController::startAnimation(tStackedWidgetAnimation* animation) {
    if (d->currentAnimation) {
        d->currentAnimation->stopAnimation();
    }

    d->currentAnimation = animation;
    connect(animation, &tStackedWidgetAnimation::done, this, &tStackedWidgetAnimationController::done);
    animation->startAnimation();
}

void tStackedWidgetAnimationController::stopCurrentAnimation() {
    if (d->currentAnimation) d->currentAnimation->stopAnimation();
}
