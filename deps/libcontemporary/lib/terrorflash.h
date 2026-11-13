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

#ifndef TERRORFLASH_H
#define TERRORFLASH_H

#include "libcontemporary_global.h"
#include <QObject>

struct tErrorFlashPrivate;
class LIBCONTEMPORARY_EXPORT tErrorFlash : public QObject {
        Q_OBJECT
    public:
        static void flashError(QWidget* flash);
        static void flashError(QWidget* flash, QString description);

    signals:
        void startingFlash(QWidget* flash);

    public slots:

    private:
        explicit tErrorFlash(QObject* parent = nullptr);
        static tErrorFlashPrivate* d;
};

#endif // TERRORFLASH_H
