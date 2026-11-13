/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#ifndef TNOTIFICATION_H
#define TNOTIFICATION_H

#include "libcontemporary_global.h"
#include <QObject>
#include <QMap>
#include <QVariant>
#include <QCoreApplication>

#ifdef T_OS_UNIX_NOT_MAC
    #include <QDBusPendingReply>
    #include <QDBusPendingCallWatcher>
    #include <QDBusMessage>
    #include <QDBusConnection>
#endif

struct tNotificationPrivateByOS;
class LIBCONTEMPORARY_EXPORT tNotification : public QObject {
        Q_OBJECT
    public:
        enum Urgency {
            Low = 0,
            Normal = 1,
            Critical = 2
        };

        explicit tNotification(QString summary = "", QString text = "", QObject* parent = 0);
        ~tNotification();

        void setTransient(bool transient);
        bool transient();

        void setTimeout(int timeout);
        int timeout();

        void setSummary(QString summary);
        QString summary();

        void setText(QString text);
        QString text();

        void setCategory(QString category);
        QString category();

        void setSound(QString sound);
        QString sound();

        void setSoundOn(bool soundOn);
        bool soundOn();

        void setUrgency(Urgency urgency);
        Urgency urgency();

        void setAppName(QString appName);
        QString appName();

        void setAppIcon(QString appIcon);
        QString appIcon();

        void insertAction(QString key, QString text);
        void removeAction(QString key);
        void clearActions();

        void insertHint(QString key, QVariant hint);
    signals:
        void actionClicked(QString key);
        void dismissed();

    public slots:
        void post(bool deleteWhenDone = true);
        void dismiss();

    private slots:

    private:
        QString sum, txt, cat, snd, app, aIcon;
        bool isTransient = false, noSound = false;
        int time = -1;
        Urgency urg = Normal;
        QVariantMap extraHints;
        QMap<QString, QString> actions;

        tNotificationPrivateByOS* dd;

        void initialize();
        void destroy();
};

#endif // TNOTIFICATION_H
