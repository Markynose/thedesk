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
#include <assistanttools.h>
#include <QDateTime>
#include "plugin.h"

#include "chunks/mprischunk.h"
#include "dbus/notificationsinterface.h"
#include "drawer/notificationsdrawer.h"
#include "notification.h"
#include "notificationtracker.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include "statuscenter/notificationsstatuscenterpane.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <SystemJob/systemjobcontroller.h>
#include <barmanager.h>
#include <icontextchunk.h>
#include <localemanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tapplication.h>
#include <tsettings.h>

struct PluginPrivate {
        NotificationTracker* tracker;
        NotificationsInterface* interface;
        NotificationsDrawer* drawer;
        NotificationsStatusCenterPane* statusCenter;
        SystemJobController* jobController;
        MprisChunk* mprisChunk;
};

Plugin::Plugin() {
    d = new PluginPrivate();
}

Plugin::~Plugin() {
    delete d;
}

void Plugin::activate() {
    tApplication::addPluginTranslator(CNTP_TARGET_NAME);

    tSettings::registerDefaults(QDir::cleanPath(qApp->applicationDirPath() + "/../plugins/NotificationsPlugin/thedesk-notifications.conf"));
    tSettings::registerDefaults("/usr/share/defaults/thedesk-notifications.conf");

    d->tracker = new NotificationTracker();

    // Broadcast new notifications (app, icon, summary; never the body) so
    // the lock screen can show them as moons rising from the horizon
    connect(d->tracker, &NotificationTracker::newNotification, d->tracker, [](NotificationPtr notification) {
        if (!notification) return;
        QString appName, iconName;
        if (notification->application()) {
            appName = notification->application()->getProperty(QStringLiteral("Name")).toString();
            iconName = notification->application()->getProperty(QStringLiteral("Icon")).toString();
        }
        QDBusMessage signal = QDBusMessage::createSignal(QStringLiteral("/com/vicr123/theDesk/Notifications"), QStringLiteral("com.vicr123.theDesk.Notifications"), QStringLiteral("Posted"));
        signal.setArguments({appName, iconName, notification->summary()});
        QDBusConnection::sessionBus().send(signal);
    });
    // For the assistant: the last notifications, when the user asks about them
    {
        struct Recent {
                QString app, summary, body;
                QDateTime when;
        };
        auto recent = QSharedPointer<QList<Recent>>::create();
        connect(d->tracker, &NotificationTracker::newNotification, d->tracker, [recent](NotificationPtr notification) {
            if (!notification) return;
            QString app;
            if (notification->application()) app = notification->application()->getProperty(QStringLiteral("Name")).toString();
            recent->prepend({app, notification->summary(), notification->body(), QDateTime::currentDateTime()});
            if (recent->size() > 20) recent->removeLast();
        });
        AssistantTools::instance()->add({QStringLiteral("recent_notifications"),
            QStringLiteral("The user's last notifications (newest first): app, title, text, time. Use it when they ask about a message or something that popped up."),
            AssistantTools::schemaOf({}), false, [recent](QJsonObject) {
                AssistantToolResult result;
                QStringList lines;
                for (const Recent& r : std::as_const(*recent)) lines.append(QStringLiteral("- %1 · %2 · %3: %4").arg(r.when.toString(QStringLiteral("HH:mm")), r.app, r.summary, QString(r.body).simplified().left(300)));
                result.text = lines.isEmpty() ? QStringLiteral("No notifications since theDesk started.") : lines.join('\n');
                return result;
            }});
    }
    d->interface = new NotificationsInterface(d->tracker);
    d->drawer = new NotificationsDrawer(d->tracker);
    d->jobController = new SystemJobController(QDBusConnection::sessionBus());
    d->statusCenter = new NotificationsStatusCenterPane(d->tracker, d->jobController);
    StateManager::statusCenterManager()->addPane(d->statusCenter, StatusCenterManager::Informational);

    d->mprisChunk = new MprisChunk();
}

void Plugin::deactivate() {
    StateManager::statusCenterManager()->removePane(d->statusCenter);

    AssistantTools::instance()->remove(QStringLiteral("recent_notifications"));
    d->tracker->deleteLater();
    d->statusCenter->deleteLater();
    d->mprisChunk->deleteLater();
    d->jobController->deleteLater();

    // Everything else will delete itself once the tracker is gone

    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
