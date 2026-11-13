/****************************************
 *
 *   theDesk - what a new display shows until it's set up
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef DISPLAYWAITINGSCREEN_H
#define DISPLAYWAITINGSCREEN_H

#include <QWidget>

class QPainterPath;
class QScreen;
struct DisplayWaitingScreenPrivate;

/**
 * Covers a newly connected display while the question is asked on the main
 * screen: the loading screen's living type (its own neutral phrases; it may
 * be a projector) with, in a calm middle, the laptop (or computer) in a
 * morphing M3 shape and what to do.
 */
class DisplayWaitingScreen : public QWidget {
        Q_OBJECT
    public:
        enum Kind {
            Waiting, // on a display that was just connected: the field, the shape, the words
            Twin,    // on the main screen as Mirror is chosen: fades in, the shape grows from the drop
            Cover    // after the displays changed: the shape alone, as the drop left it
        };

        explicit DisplayWaitingScreen(QScreen* screen, bool laptop, Kind kind = Waiting, QWidget* parent = nullptr);
        ~DisplayWaitingScreen() override;

        void dismiss(); // fades out, then deletes itself

        // The choice travels here as a drop from the main screen
        // (DisplayHandoff): its outline in global coordinates while it's on
        // the way; where it's headed; what happens when it arrives
        QScreen* shownOn() const;
        QPointF shapeCentre() const;
        void setDrop(const QPainterPath& drop);
        void land(); // the words go, the shape takes the drop's colour and swells
        void open(); // the shape becomes a window onto the desktop and grows; then this deletes itself

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        DisplayWaitingScreenPrivate* d;
};

#endif // DISPLAYWAITINGSCREEN_H
