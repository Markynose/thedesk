/****************************************
 *
 *   theDesk - the screen's backlight
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef BACKLIGHT_H
#define BACKLIGHT_H

/**
 * The built-in screen's brightness, 0..1. Through X (RandR's Backlight
 * property) when the driver has it; else the kernel's backlight
 * (/sys/class/backlight), set through logind (Session.SetBrightness), which
 * needs no root: the modesetting driver has no RandR backlight, so the old
 * way did nothing on most machines.
 */
namespace Backlight {
    bool available();
    double value();
    void set(double value);
} // namespace Backlight

#endif // BACKLIGHT_H
