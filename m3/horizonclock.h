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
#ifndef HORIZONCLOCK_H
#define HORIZONCLOCK_H

#include <QTime>

/**
 * Which horizon scene is showing at a given time of day. Shared by the living
 * wallpaper (libthedesk's HorizonScene) and the Material 3 colour seed, so the
 * UI tint always matches the sky. Scene indices match HorizonScene::SceneId.
 */
namespace HorizonClock {
    enum Scene {
        DuskOcean = 0,
        Dune,
        AuroraNight,
        Ember,
        SceneCount
    };

    struct Blend {
            int from = AuroraNight;
            int to = AuroraNight;
            double progress = 0; // 0 = all "from", 1 = all "to"
    };

    Blend blendForTime(QTime time);
} // namespace HorizonClock

#endif // HORIZONCLOCK_H
