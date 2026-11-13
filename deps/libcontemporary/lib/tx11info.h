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
#ifndef TX11INFO_H
#define TX11INFO_H

#include "libcontemporary_global.h"
#include "tapplication.h"

#if QT_CONFIG(xcb) || defined(Q_CLANG_QDOC)
    #define HAVE_TX11INFO
#endif

#ifndef HAVE_TX11INFO
typedef void Display;
#endif

typedef unsigned long Window;
class LIBCONTEMPORARY_EXPORT tX11Info : public QObject {
        Q_OBJECT
    public:
        static bool isPlatformX11();
        static Display* display();
        static Window appRootWindow();
        static unsigned long appTime();

    Q_SIGNALS:

    private:
        explicit tX11Info(QObject* parent = nullptr);
};

#endif // TX11INFO_H
