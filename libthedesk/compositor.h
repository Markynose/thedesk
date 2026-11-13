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
#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "libthedesk_global.h"
#include <QObject>

/**
 * Whether a compositing manager (picom) runs on this X screen: it owns the
 * _NET_WM_CM_S0 selection. With one, theDesk's windows can be translucent and
 * let picom round their corners (antialiased, with the shadow following);
 * without one they fall back to window masks and screen snapshots.
 * Checked every two seconds, so starting or stopping picom is noticed.
 */
class LIBTHEDESK_EXPORT Compositor : public QObject {
        Q_OBJECT
    public:
        static Compositor* instance();
        static bool isActive();

    signals:
        void activeChanged(bool active);

    private:
        explicit Compositor(QObject* parent = nullptr);
        bool active = false;
        bool check();
};

#endif // COMPOSITOR_H
