#include "tstackedwidgetanimationlift.h"

#include "tstackedwidget.h"
#include <QWidget>
#include "libcontemporary_global.h"
#include <QEasingCurve>
#include <QPointer>
#include <QGraphicsOpacityEffect>
#include <QSequentialAnimationGroup>

struct tStackedWidgetAnimationLiftPrivate {
        QParallelAnimationGroup* animation;
};

tStackedWidgetAnimationLift::tStackedWidgetAnimationLift(int oldIndex, int newIndex, tStackedWidget* parent) :
    tStackedWidgetAnimation{oldIndex, newIndex, parent} {
    d = new tStackedWidgetAnimationLiftPrivate();
}

tStackedWidgetAnimationLift::~tStackedWidgetAnimationLift() {
    delete d;
}

void tStackedWidgetAnimationLift::startAnimation() {
    if (libContemporaryCommon::isMaterial3()) {
        startFadeThrough();
        return;
    }
    this->newWidget()->setGeometry(0, this->parentStack()->height() / 8, this->parentStack()->width(), this->parentStack()->height());

    this->newWidget()->show();
    this->newWidget()->raise();

    auto* group = new QParallelAnimationGroup(this);
    auto* animation = new tPropertyAnimation(this->newWidget(), "geometry", this);
    animation->setStartValue(this->newWidget()->geometry());
    animation->setEndValue(QRect(0, 0, this->parentStack()->width(), this->parentStack()->height()));
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setDuration(250);
    connect(this->parentStack(), &tStackedWidget::resized, animation, [this, animation] {
        animation->setEndValue(QRect(0, 0, this->parentStack()->width(), this->parentStack()->height()));
    });
    group->addAnimation(animation);

    auto* effect = new QGraphicsOpacityEffect();
    effect->setOpacity(0);
    this->newWidget()->setGraphicsEffect(effect);

    auto* anim = new tPropertyAnimation(effect, "opacity", this);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setDuration(250);
    group->addAnimation(anim);

    connect(group, &QParallelAnimationGroup::finished, this, [this] {
        this->emitDone();
        this->deleteLater();
    });
    group->start();
    d->animation = group;
}

void tStackedWidgetAnimationLift::startFadeThrough() {
    // M3 "fade through" (between a navigation list's destinations): the old
    // page fades out quickly, then the new one fades in rising 30 dp, on
    // emphasized decelerate; they never show through each other
    QWidget* oldWidget = this->oldWidget();
    QWidget* newWidget = this->newWidget();
    const QRect home(0, 0, this->parentStack()->width(), this->parentStack()->height());
    QEasingCurve decelerate(QEasingCurve::BezierSpline);
    decelerate.addCubicBezierSegment(QPointF(0.05, 0.7), QPointF(0.1, 1), QPointF(1, 1));

    auto* group = new QParallelAnimationGroup(this);
    if (oldWidget) {
        auto* oldEffect = new QGraphicsOpacityEffect();
        oldEffect->setOpacity(1);
        oldWidget->setGraphicsEffect(oldEffect);
        auto* out = new tPropertyAnimation(oldEffect, "opacity", this);
        out->setStartValue(1.0);
        out->setEndValue(0.0);
        out->setDuration(90);
        group->addAnimation(out);
    }

    newWidget->setGeometry(home.translated(0, 30));
    auto* effect = new QGraphicsOpacityEffect();
    effect->setOpacity(0);
    newWidget->setGraphicsEffect(effect);
    newWidget->show();
    newWidget->raise();

    auto* incoming = new QSequentialAnimationGroup(this);
    incoming->addPause(90);
    auto* rise = new QParallelAnimationGroup(this);
    auto* geometry = new tPropertyAnimation(newWidget, "geometry", this);
    geometry->setStartValue(home.translated(0, 30));
    geometry->setEndValue(home);
    geometry->setEasingCurve(decelerate);
    geometry->setDuration(300);
    connect(this->parentStack(), &tStackedWidget::resized, geometry, [this, geometry] {
        geometry->setEndValue(QRect(0, 0, this->parentStack()->width(), this->parentStack()->height()));
    });
    rise->addAnimation(geometry);
    auto* fade = new tPropertyAnimation(effect, "opacity", this);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(decelerate);
    fade->setDuration(210);
    rise->addAnimation(fade);
    incoming->addAnimation(rise);
    group->addAnimation(incoming);

    QPointer<QWidget> oldPage = oldWidget;
    QPointer<QWidget> newPage = newWidget;
    connect(group, &QParallelAnimationGroup::finished, this, [this, oldPage, newPage] {
        // Pages keep no effects (an opacity effect left on would keep them
        // half-transparent, and costs a pixmap per paint)
        if (oldPage) oldPage->setGraphicsEffect(nullptr);
        if (newPage) newPage->setGraphicsEffect(nullptr);
        this->emitDone();
        this->deleteLater();
    });
    group->start();
    d->animation = group;
}

void tStackedWidgetAnimationLift::stopAnimation() {
    d->animation->stop();
    this->deleteLater();
}
