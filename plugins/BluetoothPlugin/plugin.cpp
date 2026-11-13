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
#include "plugin.h"

#include "btobex.h"
#include "chunk/bluetoothchunk.h"
#include "iphone/iphonelink.h"
#include "iphone/callcard.h"
#include <QTimer>
#include <tlogger.h>
#include "settings/bluetoothsettingspane.h"
#include <QDebug>
#include <QDir>
#include <localemanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tapplication.h>
#include <tsettings.h>

#include <BluezQt/InitManagerJob>
#include <BluezQt/PendingCall>

struct PluginPrivate {
        BluezQt::ManagerPtr manager;
        BtAgent* agent;
        BtObex* obex;
        BluetoothSettingsPane* bluetoothSettings;
        BluetoothChunk* chunk;
        IPhoneLink* iphone = nullptr;
};

Plugin::Plugin() {
    d = new PluginPrivate();
}

Plugin::~Plugin() {
    delete d;
}

void Plugin::activate() {
    tApplication::addPluginTranslator(CNTP_TARGET_NAME);

    tSettings::registerDefaults(QDir::cleanPath(qApp->applicationDirPath() + "/../plugins/BluetoothPlugin/thedesk-bluetooth.conf"));
    tSettings::registerDefaults("/usr/share/defaults/thedesk-bluetooth.conf");

    d->manager = BluezQt::ManagerPtr(new BluezQt::Manager());
    d->agent = new BtAgent();

    d->bluetoothSettings = new BluetoothSettingsPane(d->manager, d->agent);
    StateManager::statusCenterManager()->addPane(d->bluetoothSettings, StatusCenterManager::SystemSettings);

    d->chunk = new BluetoothChunk(d->manager);
    d->obex = new BtObex(d->manager);

    // Testing: a stand-in call rings a few seconds after starting
    if (qEnvironmentVariableIsSet("TD_CALL_TEST")) {
        QTimer::singleShot(qEnvironmentVariableIntValue("TD_CALL_TEST") * 1000, this, [] {
            if (CallCard::available()) new CallCard(QStringLiteral("Sam"), QStringLiteral("iPhone"), [](bool answer) {
                tInfo("CallCard") << "Test call" << (answer ? "answered" : "declined");
            });
        });
    }

    BluezQt::PendingCall* startCall = BluezQt::Manager::startService();
    connect(startCall, &BluezQt::PendingCall::finished, this, [=] {
        BluezQt::InitManagerJob* initManagerJob = d->manager->init();
        connect(initManagerJob, &BluezQt::InitManagerJob::result, this, [=] {
            // An iPhone's notifications (not from a nested test session:
            // it shares the real Bluetooth adapter)
            if (!qEnvironmentVariableIsSet("TD_NESTED_TEST")) {
                d->iphone = new IPhoneLink(d->manager, this);
                connect(d->iphone, &IPhoneLink::batteryChanged, d->chunk, &BluetoothChunk::refresh);
            }

            BluezQt::PendingCall* agentRegister = d->manager->registerAgent(d->agent);
            connect(agentRegister, &BluezQt::PendingCall::finished, this, [=] {
                d->manager->requestDefaultAgent(d->agent);
            });
        });
        initManagerJob->start();
    });
}

void Plugin::deactivate() {
    if (d->iphone) d->iphone->deleteLater();
    d->obex->deleteLater();
    d->agent->deleteLater();
    d->chunk->deleteLater();
    StateManager::statusCenterManager()->removePane(d->bluetoothSettings);
    d->bluetoothSettings->deleteLater();
    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
