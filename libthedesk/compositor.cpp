/****************************************
 *
 *   theDesk - compositor detection
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "compositor.h"

#include <QTimer>
#include <tx11info.h>

#ifdef HAVE_X11
    // Xlib last: its macros clash with Qt headers
    #include <X11/Xlib.h>
#endif

Compositor::Compositor(QObject* parent) :
    QObject(parent) {
    active = check();
    auto* timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, [this] {
        const bool now = check();
        if (now == active) return;
        active = now;
        emit activeChanged(active);
    });
    // Started from the event loop: the dispatcher may not exist yet
    QMetaObject::invokeMethod(timer, qOverload<>(&QTimer::start), Qt::QueuedConnection);
}

Compositor* Compositor::instance() {
    static Compositor* compositor = new Compositor();
    return compositor;
}

bool Compositor::isActive() {
    return instance()->active;
}

bool Compositor::check() {
#ifdef HAVE_X11
    if (!tX11Info::isPlatformX11()) return true; // Wayland always composites
    Display* display = tX11Info::display();
    const Atom selection = XInternAtom(display, "_NET_WM_CM_S0", False);
    return XGetSelectionOwner(display, selection) != 0;
#else
    return false;
#endif
}
