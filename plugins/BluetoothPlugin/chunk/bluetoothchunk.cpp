/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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
#include "bluetoothchunk.h"

#include <statemanager.h>
#include <barmanager.h>
#include <QIcon>
#include <BluezQt/Battery>
#include <BluezQt/Device>
#include <BluezQt/PendingCall>
#include <QSet>
#include "../iphone/iphonelink.h"
#include <aroundyou.h>
#include <statuscentermanager.h>

struct BluetoothChunkPrivate {
    BluezQt::ManagerPtr manager;
    QSet<QString> published;
    QSet<QString> connected;
    bool started = false;
};

BluetoothChunk::BluetoothChunk(BluezQt::ManagerPtr manager) : IconTextChunk("bluetooth") {
    d = new BluetoothChunkPrivate();
    d->manager = manager;

    this->setIcon(QIcon::fromTheme("bluetooth"));

    connect(manager.data(), &BluezQt::Manager::deviceAdded, this, &BluetoothChunk::updateChunk);
    connect(manager.data(), &BluezQt::Manager::deviceRemoved, this, &BluetoothChunk::updateChunk);
    connect(manager.data(), &BluezQt::Manager::deviceChanged, this, &BluetoothChunk::updateChunk);
    connect(manager.data(), &BluezQt::Manager::bluetoothOperationalChanged, this, &BluetoothChunk::updateChunk);
    updateChunk();
}

BluetoothChunk::~BluetoothChunk() {
    if (StateManager::barManager()->isChunkRegistered(this)) StateManager::barManager()->removeChunk(this);
    delete d;
}

void BluetoothChunk::refresh() {
    updateChunk();
}

void BluetoothChunk::updateChunk() {
    QVector<BluezQt::DevicePtr> activeDevices;
    for (const BluezQt::DevicePtr& device : d->manager->devices()) {
        if (device->isConnected()) activeDevices.append(device);
    }

    // Around you: the connected devices (announced as they connect), and
    // the paired ones nearby while Bluetooth is on (dragged in to connect)
    QSet<QString> seen, connectedNow;
    QVector<BluezQt::DevicePtr> shown = activeDevices;
    if (d->manager->isBluetoothOperational()) {
        for (const BluezQt::DevicePtr& device : d->manager->devices()) {
            if (device->isPaired() && !device->isConnected()) shown.append(device);
        }
    }
    for (const BluezQt::DevicePtr& device : std::as_const(shown)) {
        const QString id = QStringLiteral("bluetooth:") + device->address();
        seen.insert(id);
        AroundYouDevice entry;
        entry.id = id;
        switch (device->type()) {
            case BluezQt::Device::Headphones:
            case BluezQt::Device::Headset:
            case BluezQt::Device::AudioVideo:
                entry.symbol = QStringLiteral("headphones");
                break;
            case BluezQt::Device::Keyboard:
                entry.symbol = QStringLiteral("keyboard");
                break;
            case BluezQt::Device::Mouse:
                entry.symbol = QStringLiteral("mouse");
                break;
            case BluezQt::Device::Phone:
                entry.symbol = QStringLiteral("smartphone");
                break;
            case BluezQt::Device::Joypad:
                entry.symbol = QStringLiteral("stadia_controller");
                break;
            case BluezQt::Device::Computer:
                entry.symbol = QStringLiteral("computer");
                break;
            default:
                entry.symbol = QStringLiteral("bluetooth");
        }
        // A linked iPhone: its notifications show here
        const bool iphone = IPhoneLink::instance() && IPhoneLink::instance()->isLinked(device->address());
        if (iphone) entry.symbol = QStringLiteral("phone_iphone");
        entry.name = device->name();
        entry.connected = device->isConnected();
        if (entry.connected) connectedNow.insert(id);
        // A linked iPhone's battery comes from its own battery service (BlueZ's
        // general reading can be off by a lot)
        const int phoneBattery = iphone ? IPhoneLink::instance()->battery(device->address()) : -1;
        const int percentage = phoneBattery >= 0 ? phoneBattery : device->battery() ? device->battery()->percentage() : -1;
        entry.state = !entry.connected ? tr("Paired") : percentage >= 0 ? tr("Connected \u00b7 %1% battery").arg(percentage) : tr("Connected");
        entry.activate = [] {
            StateManager::statusCenterManager()->showWithPane(QStringLiteral("BluetoothSettings"));
        };
        entry.actionSymbol = QStringLiteral("link_off");
        BluezQt::DevicePtr pointer = device;
        entry.action = [pointer] {
            pointer->disconnectFromDevice();
        };
        entry.disconnect = entry.action;
        entry.connect = [pointer] {
            pointer->connectToDevice();
        };
        AroundYou::instance()->setDevice(entry, d->started && entry.connected && !d->connected.contains(id));
    }
    for (const QString& id : std::as_const(d->published)) {
        if (!seen.contains(id)) AroundYou::instance()->removeDevice(id);
    }
    d->published = seen;
    d->connected = connectedNow;
    d->started = true;

    if (activeDevices.isEmpty()) {
        if (StateManager::barManager()->isChunkRegistered(this)) StateManager::barManager()->removeChunk(this);
    } else {
        if (activeDevices.count() == 1) {
            this->setText(activeDevices.first()->name());
        } else {
            this->setText(tr("%n devices connected", nullptr, activeDevices.count()));
        }

        if (!StateManager::barManager()->isChunkRegistered(this)) StateManager::barManager()->addChunk(this);
    }
}
