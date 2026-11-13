/****************************************
 *
 *   theDesk - the horizon's clock
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "horizonclock.h"

#include <iterator>

HorizonClock::Blend HorizonClock::blendForTime(QTime time) {
    // (hour, scene) keyframes; consecutive keyframes crossfade
    struct Key {
            double hour;
            int scene;
    };
    static const Key keys[] = {
        {0.0,  AuroraNight},
        {5.0,  AuroraNight},
        {7.0,  DuskOcean}, // dawn
        {10.0, Dune},      // day
        {16.5, Dune},
        {18.5, DuskOcean}, // golden hour
        {20.0, Ember},     // after sunset
        {21.5, AuroraNight},
        {24.0, AuroraNight},
    };
    double hour = time.hour() + time.minute() / 60.0 + time.second() / 3600.0;
    Blend blend;
    for (size_t i = 0; i + 1 < std::size(keys); i++) {
        if (hour >= keys[i].hour && hour < keys[i + 1].hour) {
            blend.from = keys[i].scene;
            blend.to = keys[i + 1].scene;
            double span = keys[i + 1].hour - keys[i].hour;
            blend.progress = blend.from == blend.to ? 0 : (hour - keys[i].hour) / span;
            return blend;
        }
    }
    return blend;
}
