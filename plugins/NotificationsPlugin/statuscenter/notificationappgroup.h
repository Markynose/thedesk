/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#ifndef NOTIFICATIONAPPGROUP_H
#define NOTIFICATIONAPPGROUP_H

#include "notification.h"
#include <Applications/application.h>
#include <QWidget>
#include <SystemJob/systemjob.h>

/**
 * An app's notifications as its shape and a deck of cards: the newer ones
 * peek out under the top card. Tapping the shape deals the deck out; a card
 * that goes is pulled back into the shape, and an empty group spins away.
 */
struct NotificationAppGroupPrivate;
class NotificationAppGroup : public QWidget {
        Q_OBJECT

    public:
        explicit NotificationAppGroup(ApplicationPointer application, QWidget* parent = nullptr);
        ~NotificationAppGroup();

        void pushNotification(NotificationPtr notification);
        void pushJob(SystemJobPtr job);
        void dismissAll();

    signals:
        // nothing left: it's on its way out
        void emptied();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:
        NotificationAppGroupPrivate* d;

        void animate();
        void frame();
        void restack();
        QPointF shapeCentre() const;
};

#endif // NOTIFICATIONAPPGROUP_H
