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

#include "tnotification-linux.h"
#include "tnotification.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

struct tNotificationPrivateByOS {
        tNotificationLinuxHelper* h;
        uint replace = 0;

        bool deleteWhenDone = false;
};

void tNotification::post(bool deleteWhenDone) {
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");

    QVariantMap hints;
    hints.insert("transient", this->isTransient);
    hints.insert("suppress-sound", this->noSound);
    hints.insert("category", this->cat);
    hints.insert("urgency", (int) urg);

    if (this->snd != "") {
        hints.insert("sound-file", this->snd);
    }

    hints.insert("desktop-entry", QApplication::desktopFileName());
    for (const QString& key : extraHints.keys()) {
        hints.insert(key, extraHints.value(key));
    }

    QStringList actions;
    for (const QString& key : this->actions.keys()) {
        actions << key << this->actions.value(key);
    }

    QVariantList args;
    if (this->app == "") {
        args.append(QCoreApplication::applicationName());
    } else {
        args.append(this->app);
    }
    args.append(dd->replace);
    args.append(aIcon);
    args.append(sum);
    args.append(txt);
    args.append(actions);
    args.append(hints);
    args.append(time);
    message.setArguments(args);

    dd->deleteWhenDone = deleteWhenDone;

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* watcher) {
        QDBusPendingReply<uint> reply = *watcher;
        if (!reply.isError()) {
            dd->replace = reply.argumentAt<0>();
            dd->h->setListeningId(dd->replace);
        }

        watcher->deleteLater();
    });
}

void tNotification::dismiss() {
    dd->h->dismiss();
}

void tNotification::initialize() {
    dd = new tNotificationPrivateByOS();
    dd->h = new tNotificationLinuxHelper();

    QDBusConnection::sessionBus().connect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "NotificationClosed", dd->h, SLOT(dismissed(uint)));
    QDBusConnection::sessionBus().connect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "ActionInvoked", dd->h, SLOT(actionClicked(uint, QString)));
    connect(dd->h, &tNotificationLinuxHelper::didClick, this, &tNotification::actionClicked);
    connect(dd->h, &tNotificationLinuxHelper::didDismiss, this, [this] {
        emit dismissed();

        if (dd->deleteWhenDone) {
            this->deleteLater();
        }
    });
}

void tNotification::destroy() {
    QDBusConnection::sessionBus().disconnect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "NotificationClosed", dd->h, SLOT(dismissed(uint)));
    QDBusConnection::sessionBus().disconnect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "ActionInvoked", dd->h, SLOT(actionClicked(uint, QString)));

    dd->h->deleteLater();
    delete dd;
}

tNotificationLinuxHelper::tNotificationLinuxHelper(QObject* parent) :
    QObject(parent) {
}

tNotificationLinuxHelper::~tNotificationLinuxHelper() {
}

void tNotificationLinuxHelper::actionClicked(uint id, QString key) {
    if (this->currentId == id) {
        emit didClick(key);
    }
}

void tNotificationLinuxHelper::dismissed(uint id) {
    if (this->currentId == id) {
        emit didDismiss();
    }
}

void tNotificationLinuxHelper::setListeningId(uint id) {
    this->currentId = id;
}

void tNotificationLinuxHelper::dismiss() {
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "CloseNotification");
    message.setArguments({this->currentId});
    QDBusConnection::sessionBus().asyncCall(message);
}
