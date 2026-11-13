/****************************************
 *
 *   theDesk - focus tide
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef FOCUSTIDE_H
#define FOCUSTIDE_H

#include "libthedesk_global.h"
#include <QObject>

struct FocusTidePrivate;

/**
 * A focus session shown as a tide: light slowly crossing the bar and the
 * horizon. Notifications are held back (quiet mode "critical only") while
 * it runs; when the tide turns, a HUD says how many waited.
 */
class LIBTHEDESK_EXPORT FocusTide : public QObject {
        Q_OBJECT
    public:
        static FocusTide* instance();

        bool isActive() const;
        double progress() const; // 0..1, or -1 when inactive

        void start(int minutes = -1); // -1: Focus/minutes setting (default 25)
        void stop();
        void toggle();

    signals:
        void progressChanged(double progress); // -1 when it ends
        void started();
        void finished(int heldNotifications);

    private slots:
        void notificationPosted(QString appName, QString iconName, QString summary);

    private:
        explicit FocusTide(QObject* parent = nullptr);
        ~FocusTide();
        FocusTidePrivate* d;

        void end(bool completed);
};

#endif // FOCUSTIDE_H
