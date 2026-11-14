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
#include <QRegularExpression>
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
#include <quickswitch.h>
#include <quietmodemanager.h>
#include <QBoxLayout>
#include <QPushButton>

struct PluginPrivate {
        NotificationTracker* tracker;
        NotificationsInterface* interface;
        NotificationsDrawer* drawer;
        NotificationsStatusCenterPane* statusCenter;
        SystemJobController* jobController;
        MprisChunk* mprisChunk;
        QuickSwitch* quietSwitch;
        QWidget* quietModes;
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
    // Pebble's away count and the focus tide can count them
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
                QString icon;
        };
        auto recent = QSharedPointer<QList<Recent>>::create();
        connect(d->tracker, &NotificationTracker::newNotification, d->tracker, [recent](NotificationPtr notification) {
            if (!notification) return;
            QString app, icon;
            if (notification->application()) {
                app = notification->application()->getProperty(QStringLiteral("Name")).toString();
                icon = notification->application()->getProperty(QStringLiteral("Icon")).toString();
            }
            recent->prepend({app, notification->summary(), notification->body(), QDateTime::currentDateTime(), icon});
            if (recent->size() > 20) recent->removeLast();
        });
        AssistantTools::instance()->add({QStringLiteral("recent_notifications"),
            QStringLiteral("The user's last notifications (newest first): app, title, text, time. Use it when they ask about a message or something that popped up."),
            AssistantTools::schemaOf({}), false, [recent](QJsonObject) {
                AssistantToolResult result;
                QStringList lines;
                for (const Recent& r : std::as_const(*recent)) {
                    lines.append(QStringLiteral("- %1 · %2 · %3: %4").arg(r.when.toString(QStringLiteral("HH:mm")), r.app, r.summary, QString(r.body).simplified().left(300)));
                    // each one a tile in the panel (tabs would split the fields)
                    if (result.lines.size() < 8) result.lines.append(QStringList{r.app, r.icon, r.summary, QString(r.body).remove(QRegularExpression(QStringLiteral("<[^>]*>"))).simplified().left(200), r.when.toString(QStringLiteral("HH:mm"))}.replaceInStrings(QStringLiteral("\t"), QStringLiteral(" ")).join('\t'));
                }
                result.text = lines.isEmpty() ? QStringLiteral("No notifications since theDesk started.") : lines.join('\n') + QStringLiteral("\nThey're shown as tiles in the panel.");
                if (!result.lines.isEmpty()) {
                    result.card = QStringLiteral("notifications");
                    result.title = QStringLiteral("Notifications");
                }
                return result;
            }});
    }
    d->interface = new NotificationsInterface(d->tracker);
    d->drawer = new NotificationsDrawer(d->tracker);
    d->jobController = new SystemJobController(QDBusConnection::sessionBus());
    d->statusCenter = new NotificationsStatusCenterPane(d->tracker, d->jobController);
    StateManager::statusCenterManager()->addPane(d->statusCenter, StatusCenterManager::Informational);

    d->mprisChunk = new MprisChunk();

    // Do not disturb, as a Status Center tile: no notifications or back to
    // sound; its detail holds every quiet mode
    QuietModeManagerTd* quiet = StateManager::quietModeManager();
    d->quietSwitch = new QuickSwitch(QStringLiteral("NotificationsQuiet"));
    d->quietSwitch->setTitle(tr("Do not disturb"));
    d->quietSwitch->setChecked(quiet->currentMode() != QuietModeManagerTd::Sound);
    connect(quiet, &QuietModeManagerTd::quietModeChanged, d->quietSwitch, [this](QuietModeManagerTd::QuietMode mode) {
        QSignalBlocker blocker(d->quietSwitch);
        d->quietSwitch->setChecked(mode != QuietModeManagerTd::Sound);
    });
    connect(d->quietSwitch, &QuickSwitch::toggled, quiet, [quiet](bool on) {
        quiet->setQuietMode(on ? QuietModeManagerTd::NoNotifications : QuietModeManagerTd::Sound);
    });
    d->quietModes = new QWidget();
    d->quietModes->setWindowTitle(tr("Do not disturb"));
    d->quietModes->setWindowIcon(QIcon::fromTheme(QStringLiteral("do-not-disturb-on-symbolic")));
    auto* modes = new QVBoxLayout(d->quietModes);
    modes->setContentsMargins(0, 0, 0, 0);
    modes->setSpacing(4);
    for (QuietModeManagerTd::QuietMode mode : quiet->availableQuietModes()) {
        QPushButton* button = new QPushButton(QIcon::fromTheme(QuietModeManagerTd::icon(mode)), QuietModeManagerTd::name(mode), d->quietModes);
        button->setToolTip(QuietModeManagerTd::description(mode));
        button->setProperty("m3NavItem", true);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setChecked(quiet->currentMode() == mode);
        connect(button, &QPushButton::clicked, quiet, [quiet, mode] {
            quiet->setQuietMode(mode);
        });
        connect(quiet, &QuietModeManagerTd::quietModeChanged, button, [button, mode](QuietModeManagerTd::QuietMode now) {
            button->setChecked(now == mode);
        });
        modes->addWidget(button);
    }
    modes->addStretch();
    d->quietSwitch->setDetail(d->quietModes);
    d->quietSwitch->setDetailPane(d->statusCenter->name());
    StateManager::statusCenterManager()->addSwitch(d->quietSwitch);
}

void Plugin::deactivate() {
    StateManager::statusCenterManager()->removePane(d->statusCenter);
    StateManager::statusCenterManager()->removeSwitch(d->quietSwitch);
    d->quietSwitch->deleteLater();
    d->quietModes->deleteLater();

    AssistantTools::instance()->remove(QStringLiteral("recent_notifications"));
    d->tracker->deleteLater();
    d->statusCenter->deleteLater();
    d->mprisChunk->deleteLater();
    d->jobController->deleteLater();

    // Everything else will delete itself once the tracker is gone

    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
