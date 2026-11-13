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

#ifndef TPROPERTYANIMATION_H
#define TPROPERTYANIMATION_H

#include <QObject>
#include "tvariantanimation.h"

class LIBCONTEMPORARY_EXPORT tPropertyAnimation : public tVariantAnimation
{
    Q_OBJECT

public:
    tPropertyAnimation(QObject *target, const QByteArray &propertyName, QObject *parent = Q_NULLPTR);
    ~tPropertyAnimation();

    QObject* targetObject;
    QByteArray targetName;

    using tVariantAnimation::finished;

public slots:
    void start(QAbstractAnimation::DeletionPolicy policy = KeepWhenStopped);

protected slots:
    void overtake();

private slots:
    void propertyChanged(QVariant value);

private:
};

Q_DECLARE_METATYPE(tPropertyAnimation*)

#endif // TPROPERTYANIMATION_H
