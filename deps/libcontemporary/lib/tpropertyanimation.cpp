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

#include "tpropertyanimation.h"

tPropertyAnimation::tPropertyAnimation(QObject* target, const QByteArray& propertyName, QObject* parent) :
    tVariantAnimation(parent) {
    targetObject = target;
    targetName = propertyName;

    connect(this, &tPropertyAnimation::valueChanged, this, &tPropertyAnimation::propertyChanged);
    connect(targetObject, &tPropertyAnimation::destroyed, this, &tPropertyAnimation::stop);
    connect(targetObject, &tPropertyAnimation::destroyed, this, &tPropertyAnimation::deleteLater);
    connect(this, &tPropertyAnimation::stateChanged, targetObject, [this](State newState, State oldState) {
        if (newState == Running) {
            targetObject->setProperty("t-anim:" + targetName, QVariant::fromValue(this));
        } else {
            targetObject->setProperty("t-anim:" + targetName, QVariant::fromValue(static_cast<tPropertyAnimation*>(nullptr)));
        }
    });
    connect(this, &tPropertyAnimation::finished, targetObject, [this]() {
        targetObject->setProperty("t-anim:" + targetName, QVariant::fromValue(static_cast<tPropertyAnimation*>(nullptr)));
    });
}

tPropertyAnimation::~tPropertyAnimation() {
    /*if (!targetObject->property("t-anim").value<propertyAnimationInfo*>() != NULL) {
        if (targetObject->property("t-anim").value<propertyAnimationInfo*>()->runningAnimation == this) {
            targetObject->setProperty("t-anim", "");
        }
    }*/
    disconnect(this, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)));
    disconnect(this, SIGNAL(finished()));
}

void tPropertyAnimation::start(QAbstractAnimation::DeletionPolicy policy) {
    if (targetObject->property("t-anim:" + targetName).value<tPropertyAnimation*>() == nullptr) {
        targetObject->setProperty("t-anim:" + targetName, QVariant::fromValue(this));
        tVariantAnimation::start(policy);
    } else {
        if (targetObject->property("t-anim:" + targetName).value<tPropertyAnimation*>()->targetName == targetName) {
            targetObject->property("t-anim:" + targetName).value<tPropertyAnimation*>()->overtake();
        }
        targetObject->setProperty("t-anim:" + targetName, QVariant::fromValue(this));
        tVariantAnimation::start(policy);
    }
    targetObject->setProperty("t-anim:" + targetName, QVariant::fromValue(this));
    tVariantAnimation::start(policy);
}

void tPropertyAnimation::propertyChanged(QVariant value) {
    targetObject->setProperty(targetName, value);
}

void tPropertyAnimation::overtake() {
    this->stop();
    this->deleteLater();
}
