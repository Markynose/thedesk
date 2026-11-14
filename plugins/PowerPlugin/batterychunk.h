/****************************************
 *
 *   theDesk - the bar's battery
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef BATTERYCHUNK_H
#define BATTERYCHUNK_H

#include <chunk.h>

/**
 * The bar's battery: a chip (Android 16's status-bar chip) with the level as
 * a wavy ring and the figure beside it. At 20% on battery it turns
 * errorContainer and keeps the time left on the closed bar, the ring's wave
 * drifting; at 5% a turning SoftBurst with a plug grows out of the ring.
 * Plugged in it flashes primaryContainer with a bolt, then settles.
 */
class BatteryChunk : public Chunk {
        Q_OBJECT
    public:
        explicit BatteryChunk();
        ~BatteryChunk();

        // seconds: to empty on battery, to full while charging (0: unknown);
        // state: UPower's words for anything else ("Fully Charged")
        void setBattery(int percentage, bool onBattery, bool charging, qint64 seconds, const QString& state);

        // Nobody sees the wave with the screen off: it rests
        void setScreenOff(bool off);

        // "1 h 12 min"
        static QString span(qint64 seconds);

        QString name() override;
        int expandedHeight() override;
        int statusBarHeight() override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        struct Private;
        Private* d;

        void retext(bool animate);
        void updateDrift();
        void updateCovered();
        void recolour();
        QRect ringRect() const;
};

#endif // BATTERYCHUNK_H
