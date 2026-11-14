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
#include <desksound.h>
#include "plugin.h"

#include <richtip.h>

#include "batterychunk.h"
#include "chargingripple.h"
#include <QTimer>
#include <memory>

#include "eventhandler.h"
#include "settings/powersettings.h"
#include "sleepmonitor.h"
#include <DesktopPowerProfiles/desktoppowerprofiles.h>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDBusInterface>
#include <QIcon>
#include <UPower/desktopupower.h>
#include <UPower/desktopupowerdevice.h>
#include <actionquickwidget.h>
#include <barmanager.h>
#include <icontextchunk.h>
#include <localemanager.h>
#include <quickswitch.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tapplication.h>
#include <tsettings.h>

struct PluginPrivate {
        DesktopPowerProfiles* powerProfiles;

        DesktopUPower* upower;
        BatteryChunk* powerChunk;
        EventHandler* logind;
        PowerSettings* powerSettings;
        QuickSwitch* powerStretchSwitch;
        IconTextChunk* powerStretchChunk;

        SleepMonitor* sleepMonitor;
};

Plugin::Plugin() {
    d = new PluginPrivate();
}

Plugin::~Plugin() {
    delete d;
}

void Plugin::activate() {
    tApplication::addPluginTranslator(CNTP_TARGET_NAME);

    tSettings::registerDefaults(QDir::cleanPath(qApp->applicationDirPath() + "/../plugins/PowerPlugin/thedesk-power.conf"));
    tSettings::registerDefaults("/usr/share/defaults/thedesk-power.conf");

    d->powerProfiles = new DesktopPowerProfiles();

    d->upower = new DesktopUPower(this);
    // For the assistant: the battery as the bar shows it
    AssistantTools::instance()->add({QStringLiteral("battery_status"),
        QStringLiteral("The battery: charge, charging or not, time left."),
        AssistantTools::schemaOf({}), false, [upower = d->upower](QJsonObject) {
            AssistantToolResult result;
            result.text = upower->overallStateDescription();
            if (result.text.isEmpty()) result.text = QStringLiteral("No battery.");
            // a card: the charge as a ring, what it's doing, the time left
            QDBusInterface device(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"), QStringLiteral("org.freedesktop.UPower.Device"), QDBusConnection::systemBus());
            if (device.property("IsPresent").toBool()) {
                const uint state = device.property("State").toUInt(); // 1 charging, 2 on battery, 4 full
                const qint64 toEmpty = device.property("TimeToEmpty").toLongLong(), toFull = device.property("TimeToFull").toLongLong();
                result.card = QStringLiteral("battery");
                result.value = device.property("Percentage").toDouble() / 100;
                result.icon = state == 1 ? QStringLiteral("charging") : QStringLiteral("battery");
                result.title = state == 1 ? tr("Charging") : state == 4 ? tr("Full") : state == 2 ? tr("On battery") : tr("Plugged in");
                result.subtitle = state == 1 && toFull > 0 ? tr("Full in %1").arg(BatteryChunk::span(toFull)) : state == 2 && toEmpty > 0 ? tr("%1 left").arg(BatteryChunk::span(toEmpty)) : QString();
            }
            return result;
        }});
    d->powerChunk = new BatteryChunk();
    // Testing without touching real power: TD_BATTERY_TEST=50,20,10,5,6c steps through
    // these levels 4 s apart (c: charging), each with a made-up time left
    const QStringList test = qEnvironmentVariable("TD_BATTERY_TEST").split(QLatin1Char(','), Qt::SkipEmptyParts);
    auto step = std::make_shared<int>(0);
    auto follow = [this, test, step] {
        DesktopUPowerDevice* battery = d->upower->displayDevice();
        int percentage = battery->percentage();
        DesktopUPowerDevice::DeviceState state = battery->state();
        qint64 seconds = state == DesktopUPowerDevice::Charging ? battery->timeToFull() : battery->timeToEmpty();
        QString words = battery->stateString();
        if (!test.isEmpty()) {
            const QString now = test.at(qMin(*step, int(test.size()) - 1)).trimmed();
            percentage = now.chopped(now.endsWith(QLatin1Char('c')) ? 1 : 0).toInt();
            state = now.endsWith(QLatin1Char('c')) ? DesktopUPowerDevice::Charging : DesktopUPowerDevice::Discharging;
            seconds = state == DesktopUPowerDevice::Charging ? (100 - percentage) * 75 : percentage * 216;
            words = QString();
        }
        const bool onBattery = state == DesktopUPowerDevice::Discharging || state == DesktopUPowerDevice::Empty;
        const bool charging = state == DesktopUPowerDevice::Charging;
        d->powerChunk->setBattery(percentage, onBattery, charging, seconds, words);
        ChargerCorner::follow(onBattery && percentage <= 5, charging);

        const bool show = d->upower->shouldShowOverallState() || !test.isEmpty();
        if (show && !d->powerChunk->chunkRegistered()) {
            // Register the chunk
            StateManager::barManager()->addChunk(d->powerChunk);
        } else if (!show && d->powerChunk->chunkRegistered()) {
            // Deregister the chunk
            StateManager::barManager()->removeChunk(d->powerChunk);
        }
    };
    connect(d->upower, &DesktopUPower::overallStateChanged, this, follow);
    if (test.size() > 1) {
        auto* stepper = new QTimer(this);
        stepper->setInterval(4000);
        connect(stepper, &QTimer::timeout, this, [this, test, step, stepper, follow] {
            const bool wasCharging = test.at(*step).endsWith(QLatin1Char('c'));
            if (++*step >= test.size() - 1) stepper->stop();
            if (!wasCharging && test.at(*step).endsWith(QLatin1Char('c'))) ChargingRipple::play(test.at(*step).chopped(1).toInt(), 0, d->powerChunk);
            follow();
        });
        stepper->start();
    }
    d->logind = new EventHandler();
    d->logind->setBatteryChunk(d->powerChunk);
    connect(d->logind, &EventHandler::screenOffChanged, d->powerChunk, &BatteryChunk::setScreenOff);
    RichTip::attach(d->powerChunk, [this] {
        return RichTip::Content{tr("Battery"), d->upower->overallStateDescription(), tr("Power settings"), [] {
            StateManager::statusCenterManager()->showWithPane(QStringLiteral("PowerSettings"));
        }};
    });
    // Testing: the charging ripple a few seconds after starting
    if (qEnvironmentVariableIsSet("TD_CHARGING_RIPPLE_TEST")) {
        QTimer::singleShot(qEnvironmentVariableIntValue("TD_CHARGING_RIPPLE_TEST") * 1000, this, [this] {
            DeskSound::play(QStringLiteral("power-plug"));
            ChargingRipple::play(79, 4200, d->powerChunk);
        });
    }

    d->powerSettings = new PowerSettings(d->powerProfiles);
    StateManager::statusCenterManager()->addPane(d->powerSettings, StatusCenterManager::SystemSettings);

    d->powerStretchSwitch = new QuickSwitch("PowerStretch");
    d->powerStretchSwitch->setTitle(tr("Power Stretch"));
    d->powerStretchSwitch->setChecked(d->powerProfiles->currentPowerProfile() == DesktopPowerProfiles::PowerStretch);
    connect(d->powerStretchSwitch, &QuickSwitch::toggled, this, [=](bool powerStretch) {
        if (powerStretch) {
            d->powerProfiles->setCurrentPowerProfile(DesktopPowerProfiles::PowerStretch);
        } else {
            d->powerProfiles->setCurrentPowerProfile(DesktopPowerProfiles::Balanced);
        }
    });

    d->powerStretchChunk = new IconTextChunk("PowerStretch");
    d->powerStretchChunk->setIcon(QIcon::fromTheme("battery-stretch"));
    d->powerStretchChunk->setText(tr("Power Stretch"));
    if (d->powerProfiles->currentPowerProfile() == DesktopPowerProfiles::PowerStretch) StateManager::barManager()->addChunk(d->powerStretchChunk);

    ActionQuickWidget* quickWidget = new ActionQuickWidget(d->powerStretchChunk);
    quickWidget->addAction(QIcon::fromTheme("battery-stretch"), tr("Disable Power Stretch"), [=] {
        d->powerProfiles->setCurrentPowerProfile(DesktopPowerProfiles::Balanced);
    });
    d->powerStretchChunk->setQuickWidget(quickWidget);

    connect(d->powerProfiles, &DesktopPowerProfiles::powerProfileChanged, this, [=] {
        d->powerStretchSwitch->setChecked(d->powerProfiles->currentPowerProfile() == DesktopPowerProfiles::PowerStretch);
        if (d->powerProfiles->currentPowerProfile() == DesktopPowerProfiles::PowerStretch && !d->powerStretchChunk->chunkRegistered()) {
            // Register the chunk
            StateManager::barManager()->addChunk(d->powerStretchChunk);
        } else if (d->powerProfiles->currentPowerProfile() != DesktopPowerProfiles::PowerStretch && d->powerStretchChunk->chunkRegistered()) {
            // Deregister the chunk
            StateManager::barManager()->removeChunk(d->powerStretchChunk);
        }
    });
    StateManager::statusCenterManager()->addSwitch(d->powerStretchSwitch);

    d->sleepMonitor = new SleepMonitor();
}

void Plugin::deactivate() {
    AssistantTools::instance()->remove(QStringLiteral("battery_status"));
    if (d->powerChunk->chunkRegistered()) {
        // Deregister the chunk
        StateManager::barManager()->removeChunk(d->powerChunk);
    }
    if (d->powerStretchChunk->chunkRegistered()) {
        // Deregister the chunk
        StateManager::barManager()->removeChunk(d->powerStretchChunk);
    }

    StateManager::statusCenterManager()->removeSwitch(d->powerStretchSwitch);
    StateManager::statusCenterManager()->removePane(d->powerSettings);

    d->powerStretchSwitch->deleteLater();
    d->powerChunk->deleteLater();
    d->powerStretchChunk->deleteLater();
    d->upower->deleteLater();
    d->logind->deleteLater();
    d->powerSettings->deleteLater();
    d->powerProfiles->deleteLater();
    d->sleepMonitor->deleteLater();
    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
