/****************************************
 *
 *   theDesk - the charging ripple
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef CHARGINGRIPPLE_H
#define CHARGINGRIPPLE_H

#include <QPointer>
#include <QWidget>

struct ChargingRipplePrivate;

/**
 * Plugging the charger in: two wavy rings spread across the screen from
 * where the charger goes in (Power/chargingRipple.from: bottom-left,
 * bottom, bottom-right, left, right; the T460's is behind the screen's
 * bottom-left corner), sparkles riding inside them. A turning shape with a
 * bolt springs up in the middle, the level filling a wavy ring around it,
 * then shrinks into the bar's battery.
 *
 * Over everything and click-through. It needs a compositor and the
 * expressive motion scheme; play() says false otherwise (the snackbar
 * stands in).
 */
class ChargingRipple : public QWidget {
        Q_OBJECT
    public:
        // percentage 0-100; secondsToFull 0 when unknown; lands on target
        // (the bar's battery) when it's showing
        static bool play(int percentage, qint64 secondsToFull, QWidget* target);

        ~ChargingRipple() override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        ChargingRipple(int percentage, qint64 secondsToFull, QWidget* target, QScreen* screen);
        ChargingRipplePrivate* d;
        void tick();
};

#endif // CHARGINGRIPPLE_H
