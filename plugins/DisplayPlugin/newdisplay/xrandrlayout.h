/****************************************
 *
 *   theDesk - laying out displays, the way xrandr does
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef XRANDRLAYOUT_H
#define XRANDRLAYOUT_H

#include <QList>
#include <QSize>
#include <QString>

/**
 * The few layouts the new-display question offers, applied straight with
 * RandR (the calls xrandr makes), without probing the outputs again.
 *
 * Every display other than the main one runs in its own preferred mode: a
 * TV gets a standard mode, so the driver picks the colour range it expects.
 * Mirroring scales the main screen's picture onto it (xrandr --scale-from)
 * instead of forcing the main screen's resolution on it.
 */
namespace XRandrLayout {
    enum Layout {
        Extend,
        Mirror,
        OnlyMain,
        OnlyOthers
    };

    struct Output {
            QString name;
            bool connected = false;
            bool on = false;
            QSize preferred;
            QString edidHash; // identifies the display itself
    };

    bool available(); // X11 with RandR 1.3+

    QList<Output> outputs();
    // The built-in panel (eDP, LVDS, DSI), else the primary, else the first
    // connected output
    QString mainOutput();

    // Lays out every connected display; returns whether it all applied
    bool apply(Layout layout);

    // A key for the set of displays connected now (their EDIDs)
    QString arrangementKey();
} // namespace XRandrLayout

#endif // XRANDRLAYOUT_H
