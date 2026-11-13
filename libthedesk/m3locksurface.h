/****************************************
 *
 *   theDesk - the lock and login surface
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3LOCKSURFACE_H
#define M3LOCKSURFACE_H

#include "libthedesk_global.h"
#include <QWidget>

class HorizonView;
struct M3LockSurfacePrivate;

/**
 * The one screen behind the login greeter and the lock screen: the living
 * horizon, the Stack clock top left, the user card and a password pill in
 * the middle, actions (session, power) bottom right.
 *
 * Its openings and exits hand over frame for frame:
 *   open(FromBoot)    starts on Plymouth's last frame (the Gateway mark and
 *                     the wavy bar on the night sky): the bar stretches into
 *                     the password field, the mark morphs into the user's
 *                     picture, and the horizon rises under it, colours
 *                     going from the boot theme to the live scheme
 *   open(FromLock)    the horizon rises (locking)
 *   playExitToSplash  the field shrinks into the M3 loading indicator at the
 *                     loading screen's place and size (72 px, centred) while
 *                     the rest goes; the last frame is the loading screen's
 *                     first (see writeSplashHandoff)
 *   playExitToDesktop the lock opens like the loading screen does: a morphing
 *                     M3E shape grows from the field until the desktop is
 *                     all that's left
 */
class LIBTHEDESK_EXPORT M3LockSurface : public QWidget {
        Q_OBJECT
    public:
        enum class Opening {
            Plain,
            FromBoot,
            FromLock
        };

        explicit M3LockSurface(QWidget* parent = nullptr);
        ~M3LockSurface() override;

        HorizonView* horizon() const;
        void setUser(const QString& displayName, const QString& picturePath = QString());
        void setGreeting(const QString& greeting);
        void addAction(QWidget* action); // bottom right, left of the ones before
        void setPlaceholder(const QString& text);
        // A small widget of the caller's under the date (the lock screen's
        // Claude chip): it shows, hides and sizes itself, 10 px of its left
        // edge hanging out so what it draws there lines up with the date
        void setClockNote(QWidget* note);

        void open(Opening opening);
        QString password() const;
        void clearPassword();
        void setBusy(bool busy);
        void showError(const QString& text); // the field shakes
        void showMessage(const QString& text);
        void focusPassword();

        void playExitToSplash();
        void playExitToDesktop(); // unlocking: the screen opens from the field onto the desktop
        qint64 indicatorElapsed() const; // the loading indicator's time, for the hand-off

        // For the loading screen: an X root-window property with the
        // indicator's time, read by startdesk so its indicator carries on
        static void writeSplashHandoff(QWidget* window, qint64 indicatorMs);

    signals:
        void submitted(QString password);
        void exitFinished();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        M3LockSurfacePrivate* d;
        void relayout();
};

#endif // M3LOCKSURFACE_H
