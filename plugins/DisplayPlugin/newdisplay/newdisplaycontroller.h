/****************************************
 *
 *   theDesk - a newly connected display
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef NEWDISPLAYCONTROLLER_H
#define NEWDISPLAYCONTROLLER_H

#include <QObject>

class QRect;
class SystemScreen;
struct NewDisplayControllerPrivate;

/**
 * When a display is connected that theDesk hasn't seen in this combination
 * before, it is switched on beside the others, wears the waiting screen
 * (DisplayWaitingScreen), and a sheet on the main screen asks how to use it
 * (DisplayChooser): extend, mirror, only the main screen, only the new one.
 * The answer is saved (ScreenDaemon), so the same set of displays comes back
 * the same way without asking.
 *
 * It also makes sure some screen is always on: unplugging the only display
 * that was on (after "only the new one") switches the main screen back on.
 */
class NewDisplayController : public QObject {
        Q_OBJECT
    public:
        explicit NewDisplayController(QObject* parent = nullptr);
        ~NewDisplayController() override;

        enum Choice {
            Extend,
            Mirror,
            OnlyMain,
            OnlyOther
        };

        // The plugin's (null while it isn't active)
        static NewDisplayController* instance();

        // Ask again for the displays connected now (Display settings,
        // Super+P); false when there is only one display
        bool askAgain();

        // The screen built into this machine (eDP/LVDS/DSI), else the primary
        static SystemScreen* mainScreen();
        // Whether this machine is a laptop (its chassis type)
        static bool isLaptop();

    private:
        NewDisplayControllerPrivate* d;

        void screensChanged();
        void publish(bool announce); // the displays, in Around you
        void ask();
        void apply(Choice choice);
        void finish();

        // Chosen in the sheet: the tile's drop crosses to the waiting
        // displays, which then open onto the desktop as it was chosen
        void handOver(Choice choice, const QRect& tile);
        void remember(Choice choice);
        void reveal(Choice choice);
        void coverAndOpen(Choice choice);
};

#endif // NEWDISPLAYCONTROLLER_H
