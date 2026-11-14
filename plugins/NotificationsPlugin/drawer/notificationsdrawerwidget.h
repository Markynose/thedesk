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
#ifndef NOTIFICATIONSDRAWERWIDGET_H
#define NOTIFICATIONSDRAWERWIDGET_H

#include <QPainterPath>
#include <QWidget>

class Notification;
typedef QPointer<Notification> NotificationPtr;

struct NotificationsDrawerWidgetPrivate;
class NotificationTracker;

/**
 * A pop-up as a pebble: it pops in as its app's shape, unfurls into a card
 * (the shape riding its leading edge) and folds back into a pebble in the
 * rail, a countdown ring round it. The drawer steps it from its frame timer.
 */
class NotificationsDrawerWidget : public QWidget {
        Q_OBJECT

    public:
        explicit NotificationsDrawerWidget(NotificationPtr notification, NotificationTracker* tracker, QWidget* parent = nullptr);
        ~NotificationsDrawerWidget();

        static constexpr int CardWidth = 420;
        static constexpr int Edge = 8; // room beside the plate, inside the window

        void animateDismiss();

        // The newest one is unfurled; the rest fold into the rail
        void setFront(bool front);
        bool leaving() const;
        bool done() const;

        // Steps its springs; true while anything moves
        bool step(double dt);
        int plateHeight() const;
        double slot() const;
        // What shows, in its own coordinates (the window's mask)
        QPainterPath outline() const;

    signals:
        void animate();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:
        NotificationsDrawerWidgetPrivate* d;

        void setupActions();
        void updateApplication();
        void measure();
        bool needsAnswer() const;
        QRectF plate() const;
        QPointF coin() const;
};

#endif // NOTIFICATIONSDRAWERWIDGET_H
