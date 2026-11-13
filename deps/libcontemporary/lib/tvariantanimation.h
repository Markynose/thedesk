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

#ifndef TVARIANTANIMATION_H
#define TVARIANTANIMATION_H

#include <QVariantAnimation>
#include <QDebug>
#include <functional>
#include "libcontemporary_global.h"

class LIBCONTEMPORARY_EXPORT tVariantAnimation : public QVariantAnimation {
        Q_OBJECT

    public:
        tVariantAnimation(QObject* parent = nullptr);
        ~tVariantAnimation();

        typedef std::function<void(QVariant)> ValueChangedFunction;
        typedef std::function<void()> FinishedFunction;

        void setForceAnimation(bool force);
        bool forceAnimation();

        static tVariantAnimation* singleShot(QObject* parent, QVariant start, QVariant end, int duration, QEasingCurve easingCurve, ValueChangedFunction valueChangedCallback, FinishedFunction finishedCallback);
        static tVariantAnimation* singleShot(QObject* parent, QVariant start, QVariant end, int duration, QEasingCurve easingCurve, ValueChangedFunction valueChangedCallback);
        static tVariantAnimation* singleShot(QObject* parent, QVariant start, QVariant end, int duration, ValueChangedFunction valueChangedCallback, FinishedFunction finishedCallback);
        static tVariantAnimation* singleShot(QObject* parent, QVariant start, QVariant end, int duration, ValueChangedFunction valueChangedCallback);

    public slots:
        void start(QAbstractAnimation::DeletionPolicy policy = KeepWhenStopped);

    private:
        bool forceAnim = false;
};

#endif // TVARIANTANIMATION_H
