/****************************************
 *
 *   theDesk - the level pop-up (volume, brightness)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef LEVELHUD_H
#define LEVELHUD_H

#include <QVariantMap>
#include <QWidget>

struct LevelHudPrivate;

/**
 * Volume and brightness as an M3 Expressive slider (size L) in a pill at the
 * right edge of the screen, as Android 16 does: the active track Primary
 * with the symbol inside it, a thin handle with gaps, the level in Roboto
 * Flex above. Comes in on a spring, goes after a moment untouched.
 */
class LevelHud : public QWidget {
        Q_OBJECT
    public:
        explicit LevelHud(QWidget* parent = nullptr);
        ~LevelHud() override;

        // HudManager parameters with a "value" (0..1, above 1 = boosted)
        void showLevel(const QVariantMap& parameters);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        LevelHudPrivate* d;
        void place();
};

#endif // LEVELHUD_H
