/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2019 Victor Tran
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
#include "desktopupower.h"

#include "desktopupowerdevice.h"
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QIcon>

struct DesktopUPowerPrivate {
        QDBusInterface* interface;
        QDBusInterface* tsInterface;

        QMap<QDBusObjectPath, DesktopUPowerDevice*> devices;
        bool powerStretch = false;

        // UPower's composite device: all batteries combined into one
        // percentage/state (what XFCE and GNOME show).
        DesktopUPowerDevice* displayDevice = nullptr;
};

DesktopUPower::DesktopUPower(QObject* parent) :
    QObject(parent) {
    d = new DesktopUPowerPrivate();
    d->interface = new QDBusInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus());
    d->tsInterface = new QDBusInterface("org.thesuite.theShell", "/org/thesuite/Power", "org.thesuite.Power");

    d->displayDevice = new DesktopUPowerDevice(QDBusObjectPath(QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice")), this);
    connect(d->displayDevice, &DesktopUPowerDevice::propertiesUpdated, this, &DesktopUPower::overallStateChanged);

    QDBusPendingCallWatcher* devices = new QDBusPendingCallWatcher(d->interface->asyncCall("EnumerateDevices"));
    connect(devices, &QDBusPendingCallWatcher::finished, this, [this, devices] {
        QDBusArgument arg = devices->reply().arguments().first().value<QDBusArgument>();
        QList<QDBusObjectPath> paths;
        arg >> paths;

        for (QDBusObjectPath path : paths) {
            // Add the device
            this->deviceAdded(path);
        }
        emit devicesChanged();
    });

    QDBusConnection::sessionBus().connect("org.thesuite.theshell", "/org/thesuite/Power", "org.thesuite.Power", "powerStretchChanged", this, SIGNAL(powerStretchChanged(bool)));
    QDBusPendingCallWatcher* powerStretch = new QDBusPendingCallWatcher(d->tsInterface->asyncCall("powerStretch"));
    connect(powerStretch, &QDBusPendingCallWatcher::finished, this, [this, powerStretch] {
        QDBusMessage msg = powerStretch->reply();
        if (msg.type() != QDBusMessage::ErrorMessage) {
            d->powerStretch = powerStretch->reply().arguments().first().toBool();
            emit overallStateChanged();
        }
    });
}

DesktopUPower::~DesktopUPower() {
    d->interface->deleteLater();
    delete d;
}

QList<DesktopUPowerDevice*> DesktopUPower::devices() {
    return d->devices.values();
}

bool DesktopUPower::shouldShowOverallState() {
    for (DesktopUPowerDevice* device : this->devices()) {
        if (device->type() == DesktopUPowerDevice::Battery) return true;
    }
    return false;
}

QString DesktopUPower::overallStateDescription() {
    QStringList parts;

    if (d->powerStretch) {
        parts.append(tr("Power Stretch On"));
    }

    // Prefer the combined device: one percentage and state for all batteries
    if (d->displayDevice && d->displayDevice->type() == DesktopUPowerDevice::Battery) {
        parts.append(QStringLiteral("%1%").arg(d->displayDevice->percentage()));
        parts.append(d->displayDevice->stateString());
        return parts.join(" · ");
    }

    int extraBatteriesCount = -1;
    for (DesktopUPowerDevice* device : this->devices()) {
        if (device->type() == DesktopUPowerDevice::Battery) {
            if (extraBatteriesCount < 0) {
                parts.append(QStringLiteral("%1%").arg(device->percentage()));
                parts.append(device->stateString());
            }
            extraBatteriesCount++;
        }
    }

    if (extraBatteriesCount > 0) parts.append(tr("%n more batteries", nullptr, extraBatteriesCount));

    return parts.join(" · ");
}

QIcon DesktopUPower::overallStateIcon() {
    if (d->displayDevice && d->displayDevice->type() == DesktopUPowerDevice::Battery) return d->displayDevice->icon();
    for (DesktopUPowerDevice* device : this->devices()) {
        if (device->type() == DesktopUPowerDevice::Battery) {
            return device->icon();
        }
    }
    return QIcon();
}

void DesktopUPower::deviceAdded(QDBusObjectPath device) {
    DesktopUPowerDevice* deviceObject = new DesktopUPowerDevice(device);
    d->devices.insert(device, deviceObject);
    connect(deviceObject, &DesktopUPowerDevice::propertiesUpdated, this, &DesktopUPower::overallStateChanged);
    emit deviceAdded(deviceObject);
    emit overallStateChanged();
}

void DesktopUPower::deviceRemoved(QDBusObjectPath device) {
    if (d->devices.contains(device)) {
        DesktopUPowerDevice* deviceObject = d->devices.value(device);
        deviceObject->deleteLater();
        d->devices.remove(device);

        emit deviceRemoved(deviceObject);
        emit overallStateChanged();
    }
}

void DesktopUPower::powerStretchChanged(bool isOn) {
    d->powerStretch = isOn;
    emit overallStateChanged();
}
