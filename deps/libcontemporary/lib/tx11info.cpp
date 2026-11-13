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
#include "tx11info.h"

#ifdef HAVE_TX11INFO
#include <X11/Xlib.h>
#endif

bool tX11Info::isPlatformX11() {
#ifdef HAVE_TX11INFO
    if (qApp->nativeInterface<QNativeInterface::QX11Application>()) return true;
#endif
    return false;
}

Display* tX11Info::display() {
#ifdef HAVE_TX11INFO
    return qApp->nativeInterface<QNativeInterface::QX11Application>()->display();
#else
    return nullptr;
#endif
}

Window tX11Info::appRootWindow() {
#ifdef HAVE_TX11INFO
    return DefaultRootWindow(qApp->nativeInterface<QNativeInterface::QX11Application>()->display());
#else
    return 0;
#endif
}

unsigned long tX11Info::appTime() {
#ifdef HAVE_TX11INFO
    return CurrentTime;
#else
    return 0;
#endif
}

tX11Info::tX11Info(QObject* parent) :
    QObject{parent} {
}
