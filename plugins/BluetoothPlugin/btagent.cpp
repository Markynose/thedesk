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
#include "btagent.h"

#include "iphone/iphonelink.h"

#include <QDBusObjectPath>
#include <BluezQt/Device>
#include <tlogger.h>
#include <tnotification.h>
#include "settings/pairpopover.h"

struct BtAgentPrivate {
    BluezQt::DevicePtr capturedDevice;
    PairPopover* capturedPopover;

    QPointer<tNotification> notification;
};

BtAgent::BtAgent(QObject* parent) : BluezQt::Agent(parent) {
    d = new BtAgentPrivate();
}

BtAgent::~BtAgent() {
    delete d;
}

void BtAgent::capturePairRequests(BluezQt::DevicePtr device, PairPopover* popover) {
    d->capturedDevice = device;
    d->capturedPopover = popover;
}

QDBusObjectPath BtAgent::objectPath() const {
    return QDBusObjectPath("/com/vicr123/theDesk/BluetoothPlugin/Agent");
}

BluezQt::Agent::Capability BtAgent::capability() const {
    return DisplayYesNo;
}

void BtAgent::requestPinCode(BluezQt::DevicePtr device, const BluezQt::Request<QString>& request) {
    // Typing a PIN for the device isn't supported yet: say no rather than
    // leave the device waiting
    tWarn("BtAgent") << "PIN code requested by" << device->name() << "- not supported, rejecting";
    request.reject();
}

void BtAgent::displayPinCode(BluezQt::DevicePtr device, const QString& pinCode) {
    tInfo("BtAgent") << "Showing a PIN code for" << device->name();
    if (device == d->capturedDevice) {
        d->capturedPopover->triggerPairConfirmation(PairPopover::KeyPinCode, pinCode)->error([ = ](QString error) {
            device->cancelPairing();
        });
    } else {
        showCode(device, pinCode);
    }
}

void BtAgent::requestPasskey(BluezQt::DevicePtr device, const BluezQt::Request<quint32>& request) {
    tWarn("BtAgent") << "Passkey requested by" << device->name() << "- not supported, rejecting";
    request.reject();
}

void BtAgent::displayPasskey(BluezQt::DevicePtr device, const QString& passkey, const QString& entered) {
    Q_UNUSED(entered)
    tInfo("BtAgent") << "Showing a passkey for" << device->name();
    if (device == d->capturedDevice) {
        d->capturedPopover->triggerPairConfirmation(PairPopover::KeyPinCode, passkey);
    } else if (!d->notification) {
        // The device asks for the code to be typed on it (an iPhone does)
        showCode(device, passkey);
    }
}

void BtAgent::showCode(BluezQt::DevicePtr device, const QString& code) {
    tNotification* notification = new tNotification();
    notification->setAppIcon("preferences-system-bluetooth");
    notification->setAppName(tr("Bluetooth"));
    notification->setSummary(tr("Pair with %1").arg(QLocale().quoteString(device->name())));
    notification->setText(tr("Type this code on %1:").arg(QLocale().quoteString(device->name())).append(QStringLiteral("<p><b style=\"font-size: 20pt;\">%1</b></p>").arg(code)));
    notification->insertAction("1cancel", tr("Cancel"));
    notification->setTimeout(0);
    notification->setUrgency(tNotification::Critical);
    connect(notification, &tNotification::actionClicked, this, [device](QString key) {
        if (key == "1cancel") device->cancelPairing();
    });
    // Paired: the code has done its job
    connect(device.data(), &BluezQt::Device::pairedChanged, notification, [notification](bool paired) {
        if (paired) notification->dismiss();
    });
    notification->post();
    d->notification = notification;
}

void BtAgent::requestConfirmation(BluezQt::DevicePtr device, const QString& passkey, const BluezQt::Request<>& request) {
    tInfo("BtAgent") << "Confirming a pairing code with" << device->name();
    if (device == d->capturedDevice) {
        d->capturedPopover->triggerPairConfirmation(PairPopover::ConfirmPinCode, passkey)->then([ = ](QString pinCode) {
            request.accept();
        })->error([ = ](QString error) {
            request.reject();
        });
    } else {
        tNotification* notification = new tNotification();
        notification->setAppIcon("preferences-system-bluetooth");
        notification->setAppName(tr("Bluetooth"));
        notification->setSummary(tr("Pair with %1?").arg(QLocale().quoteString(device->name())));
        notification->setText(tr("In order to complete pairing, confirm that the following pairing code is shown on %1.").arg(QLocale().quoteString(device->name())).append(QStringLiteral("<p><b style=\"font-size: 20pt;\">%1</b></p>").arg(passkey))
            .append(tr("If it is, you can accept the pairing request.")));
        notification->insertAction("1reject", tr("Decline"));
        notification->insertAction("2accept", tr("Accept and Pair"));
        notification->setTimeout(0);
        notification->setUrgency(tNotification::Critical); // it needs an answer: shows even in Critical Only
        connect(notification, &tNotification::actionClicked, this, [ = ](QString key) {
            if (key == "1reject") {
                request.reject();
            } else if (key == "2accept") {
                request.accept();
            }
        });
        connect(notification, &tNotification::dismissed, this, [ = ] {
            request.reject();
        });
        notification->post();

        d->notification = notification;
    }
}

void BtAgent::requestAuthorization(BluezQt::DevicePtr device, const BluezQt::Request<>& request) {
    tInfo("BtAgent") << "Authorising pairing with" << device->name();
    if (device == d->capturedDevice) {
        //Automatically accept the pairing request
        request.accept();
    } else {
        tNotification* notification = new tNotification();
        notification->setAppIcon("preferences-system-bluetooth");
        notification->setAppName(tr("Bluetooth"));
        notification->setSummary(tr("Pair with %1?").arg(QLocale().quoteString(device->name())));
        notification->setText(tr("Accept the pairing request from %1?").arg(QLocale().quoteString(device->name())));
        notification->insertAction("1reject", tr("Decline"));
        notification->insertAction("2accept", tr("Pair"));
        notification->setTimeout(0);
        notification->setUrgency(tNotification::Critical); // it needs an answer: shows even in Critical Only
        connect(notification, &tNotification::actionClicked, this, [ = ](QString key) {
            if (key == "1reject") {
                request.reject();
            } else if (key == "2accept") {
                request.accept();
            }
        });
        connect(notification, &tNotification::dismissed, this, [ = ] {
            request.reject();
        });
        notification->post();

        d->notification = notification;
    }
}

void BtAgent::authorizeService(BluezQt::DevicePtr device, const QString& uuid, const BluezQt::Request<>& request) {
    // A linked iPhone shares its notifications; its sound stays on it
    if (IPhoneLink::instance() && IPhoneLink::instance()->isLinked(device->address()) && IPhoneLink::isAudioProfile(uuid) && !IPhoneLink::audioAllowed()) {
        tInfo("BtAgent") << "Turning away sound from" << device->name();
        request.reject();
        return;
    }
    request.accept();
}

void BtAgent::cancel() {
    tInfo("BtAgent") << "Pairing cancelled";
    if (d->capturedDevice) {
        d->capturedPopover->cancelPendingPairing();
    } else if (d->notification) {
        d->notification->dismiss();
    }
}
