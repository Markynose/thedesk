/****************************************
 *
 *   theDesk - a chosen display mode travels to the display
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef DISPLAYHANDOFF_H
#define DISPLAYHANDOFF_H

#include <QList>
#include <QObject>
#include <QRect>

class QScreen;
class DisplayWaitingScreen;
struct DisplayHandoffPrivate;

/**
 * The tile chosen on the main screen gathers into a drop that crosses to
 * each waiting screen and ends in its shape: the choice is seen to go where
 * it takes effect. (The same liquid as the link in Around you.)
 *
 * It draws on the main screen in a window of its own and hands each waiting
 * screen the part that's on it; landed() when the drops are in, after which
 * it deletes itself.
 */
class DisplayHandoff : public QObject {
        Q_OBJECT
    public:
        DisplayHandoff(const QRect& tile, QScreen* mainScreen, const QList<DisplayWaitingScreen*>& targets, QObject* parent = nullptr);
        ~DisplayHandoff() override;

        void start();

    signals:
        void landed();

    private:
        DisplayHandoffPrivate* d;
        void frame();
};

#endif // DISPLAYHANDOFF_H
