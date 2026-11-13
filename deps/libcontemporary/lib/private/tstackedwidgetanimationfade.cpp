#include "tstackedwidgetanimationfade.h"

#include "tstackedwidget.h"
#include <QWidget>

struct tStackedWidgetAnimationFadePrivate {
        tVariantAnimation* animation;
};

tStackedWidgetAnimationFade::tStackedWidgetAnimationFade(int oldIndex, int newIndex, tStackedWidget* parent) :
    tStackedWidgetAnimation{oldIndex, newIndex, parent} {
    d = new tStackedWidgetAnimationFadePrivate();
}

tStackedWidgetAnimationFade::~tStackedWidgetAnimationFade() {
    delete d;
}

void tStackedWidgetAnimationFade::startAnimation() {
    this->newWidget()->show();
    this->newWidget()->raise();
    this->newWidget()->resize(this->parentStack()->width(), this->parentStack()->height());

    auto* effect = new QGraphicsOpacityEffect();
    effect->setOpacity(0);
    this->newWidget()->setGraphicsEffect(effect);

    auto* anim = new tVariantAnimation(this);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setDuration(250);
    connect(anim, &tVariantAnimation::valueChanged, this, [effect](const QVariant& value) {
        effect->setOpacity(value.toDouble());
    });
    connect(anim, &tPropertyAnimation::finished, this, [this] {
        this->emitDone();
        this->deleteLater();
    });
    anim->start();
    d->animation = anim;
}

void tStackedWidgetAnimationFade::stopAnimation() {
    d->animation->stop();
    this->deleteLater();
}
