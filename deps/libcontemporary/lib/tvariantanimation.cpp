/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "tvariantanimation.h"

tVariantAnimation::tVariantAnimation(QObject* parent) : QVariantAnimation(parent) {

}

tVariantAnimation::~tVariantAnimation() {

}

void tVariantAnimation::start(QAbstractAnimation::DeletionPolicy policy) {
    if (this->state() != Running) {
        if ((!libContemporaryCommon::instance()->allowSystemAnimations() || libContemporaryCommon::instance()->powerStretchEnabled()) && !forceAnim) {
            QVariant newValue;
            int newTime;
            if (this->direction() == Forward) {
                newValue = this->endValue();
                newTime = this->duration();
            } else {
                newValue = this->startValue();
                newTime = 0;
            }

            QMetaObject::invokeMethod(this, "setCurrentTime", Qt::QueuedConnection, Q_ARG(int, newTime));
            QMetaObject::invokeMethod(this, "valueChanged", Qt::QueuedConnection, Q_ARG(QVariant, newValue));
            QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
        } else {
            QVariantAnimation::start(policy);
        }
    }
}

void tVariantAnimation::setForceAnimation(bool force) {
    forceAnim = force;
}

bool tVariantAnimation::forceAnimation() {
    return forceAnim;
}

tVariantAnimation* tVariantAnimation::singleShot(QObject* parent, QVariant start, QVariant end, int duration, QEasingCurve easingCurve, tVariantAnimation::ValueChangedFunction valueChangedCallback, FinishedFunction finishedCallback) {
    tVariantAnimation* anim = new tVariantAnimation(parent);
    anim->setStartValue(start);
    anim->setEndValue(end);
    anim->setDuration(duration);
    anim->setEasingCurve(easingCurve);
    connect(anim, &tVariantAnimation::valueChanged, parent, [ = ](QVariant value) {
        valueChangedCallback(value);
    });
    connect(anim, &tVariantAnimation::finished, parent, [ = ] {
        finishedCallback();
        anim->deleteLater();
    });
    anim->start();

    return anim;
}

tVariantAnimation* tVariantAnimation::singleShot(QObject* parent, QVariant start, QVariant end, int duration, QEasingCurve easingCurve, tVariantAnimation::ValueChangedFunction valueChangedCallback) {
    return tVariantAnimation::singleShot(parent, start, end, duration, easingCurve, valueChangedCallback, [ = ] {});
}

tVariantAnimation* tVariantAnimation::singleShot(QObject* parent, QVariant start, QVariant end, int duration, tVariantAnimation::ValueChangedFunction valueChangedCallback, FinishedFunction finishedCallback) {
    return tVariantAnimation::singleShot(parent, start, end, duration, QEasingCurve::OutCubic, valueChangedCallback, finishedCallback);
}

tVariantAnimation* tVariantAnimation::singleShot(QObject* parent, QVariant start, QVariant end, int duration, tVariantAnimation::ValueChangedFunction valueChangedCallback) {
    return tVariantAnimation::singleShot(parent, start, end, duration, valueChangedCallback, [ = ] {});
}
