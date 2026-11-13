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

#ifndef TWINDOWTHUMBNAIL_H
#define TWINDOWTHUMBNAIL_H

#include "libcontemporary_global.h"
#include <QWidget>
#include <QList>
#include <QAction>

enum class tWindowThumbnailProgressState {
    NoProgress,
    Indeterminate,
    Normal,
    Paused,
    Error
};

class LIBCONTEMPORARY_EXPORT tWindowThumbnail : public QObject {
        Q_OBJECT
    public:
        static tWindowThumbnail* thumbnailFor(QWidget* parent);
        virtual ~tWindowThumbnail() { };

        virtual void setToolbar(const QList<QAction*>& actions) = 0;
        virtual void setProgressState(tWindowThumbnailProgressState state) = 0;
        virtual void setProgressValue(float percent) = 0;
    protected:
        tWindowThumbnail(QWidget* parent) : QObject(parent) { }
};

#endif // TWINDOWTHUMBNAIL_H
