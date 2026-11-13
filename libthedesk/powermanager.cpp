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
#include "powermanager.h"
#include "farewell.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QPointer>
#include <QProcess>

#include "keygrab.h"
#include <QKeySequence>
#include <Wm/desktopwm.h>
#include <tnotification.h>

#include "onboardingmanager.h"
#include "statemanager.h"

struct PowerManagerPrivate {
        QPointer<QProcess> lockScreenProcess;
        QDBusInterface* logindInterface;
};

PowerManager::PowerManager(QObject* parent) :
    QObject(parent) {
    d = new PowerManagerPrivate();

    connect(new KeyGrab(QKeySequence(Qt::Key_L | Qt::MetaModifier), "lockScreen"), &KeyGrab::activated, this, [=] {
        this->performPowerOperation(PowerManager::Lock);
    });

    // Find this session ID
    d->logindInterface = new QDBusInterface("org.freedesktop.login1", "/org/freedesktop/login1/session/self", "org.freedesktop.login1.Session", QDBusConnection::systemBus(), this);
    QString id = d->logindInterface->property("Id").toString();
    if (!id.isEmpty()) {
        QDBusMessage sessionRequest = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "GetSession");
        sessionRequest.setArguments({id});
        QDBusMessage sessionReply = QDBusConnection::systemBus().call(sessionRequest);
        if (sessionReply.type() == QDBusMessage::ReplyMessage) {
            // Register event handlers for logind
            QDBusObjectPath path = sessionReply.arguments().first().value<QDBusObjectPath>();
            QDBusConnection::systemBus().connect("org.freedesktop.login1", path.path(), "org.freedesktop.login1.Session", "Lock", this, SLOT(logindRequestLock()));
            QDBusConnection::systemBus().connect("org.freedesktop.login1", path.path(), "org.freedesktop.login1.Session", "Unlock", this, SLOT(logindRequestUnlock()));
        }
    }
}

PowerManager::~PowerManager() {
    delete d;
}

tPromise<void>* PowerManager::showPowerOffConfirmation(PowerManager::PowerOperation operation, QString message, QStringList flags) {
    return tPromise<void>::runOnSameThread([=](tPromiseFunctions<void>::SuccessFunction res, tPromiseFunctions<void>::FailureFunction rej) {
        Q_UNUSED(rej)

        emit powerOffConfirmationRequested(operation, message, flags, res);
    });
}

void PowerManager::logindRequestLock() {
    this->performPowerOperation(Lock);
}

void PowerManager::logindRequestUnlock() {
    if (d->lockScreenProcess) d->lockScreenProcess->terminate();
    // Process will be automatically deleted
}

void PowerManager::performPowerOperation(PowerManager::PowerOperation operation, QStringList flags) {
    // Shutting down, restarting and logging out say goodbye first: the
    // loading screen played the other way (Farewell), then on with it
    // A nested test session (TD_NESTED_TEST=1, set by the Xephyr test
    // scripts) shares the real system bus: never power off, reboot, suspend
    // or hibernate the real machine from it
    const bool nestedBlocked = qEnvironmentVariableIsSet("TD_NESTED_TEST") && (operation == PowerOff || operation == Reboot || operation == Suspend || operation == Hibernate);

    if ((operation == PowerOff || operation == Reboot || operation == LogOut) && !flags.contains(QStringLiteral("farewell-done"))) {
        // Tell listeners now, not after the goodbye: they act while the
        // request is still theirs (Recovery's "log out and reset" stops
        // listening as soon as the confirmation closes)
        if (!nestedBlocked) emit powerOffOperationCommencing(operation);
        const Farewell::Kind kind = operation == PowerOff ? Farewell::PowerOff : operation == Reboot ? Farewell::Reboot : Farewell::LogOut;
        Farewell::play(kind, [this, operation, flags] {
            this->performPowerOperation(operation, flags + QStringList{QStringLiteral("farewell-done")});
        });
        return;
    }

    if (nestedBlocked) {
        qWarning("theDesk: nested test session, ignoring power operation %d", int(operation));
        return;
    }

    if (!flags.contains(QStringLiteral("farewell-done"))) emit powerOffOperationCommencing(operation);

    switch (operation) {
        case PowerManager::PowerOff:
        case PowerManager::Reboot:
        case PowerManager::Suspend:
        case PowerManager::Hibernate:
            {
                QMap<PowerManager::PowerOperation, QString> methods = {
                    {PowerManager::PowerOff,  "PowerOff" },
                    {PowerManager::Reboot,    "Reboot"   },
                    {PowerManager::Suspend,   "Suspend"  },
                    {PowerManager::Hibernate, "Hibernate"}
                };

                if (flags.contains("update")) {
                    // Ask PackageKit to prepare for updates

                    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.PackageKit", "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Offline", "Trigger");
                    message.setArguments({operation == PowerManager::Reboot ? "reboot" : "shutdown"});
                    QDBusMessage msg = QDBusConnection::systemBus().call(message);

                    // Fall through
                }

                if (flags.contains("setup")) {
                    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "SetRebootToFirmwareSetup");
                    message.setArguments({true});
                    QDBusMessage msg = QDBusConnection::systemBus().call(message);

                    // Fall through
                }

                QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", methods.value(operation));
                message.setArguments({true});
                // Not blocking: it may wait on a password prompt, which the
                // desktop has to keep drawing. Refused or failed: the goodbye
                // (if any) comes down at once
                auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 120000), this);
                connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher* watcher) {
                    if (watcher->isError()) Farewell::cancel();
                    watcher->deleteLater();
                });
                break;
            }
        case PowerManager::LogOut:
            // Quit theDesk
            qApp->exit();
            break;
        case PowerManager::Lock:
            // Lock the screen
            if (d->lockScreenProcess) return;                                     // Screen is already locked
            if (StateManager::onboardingManager()->isOnboardingRunning()) return; // Onboarding is currently running

            d->lockScreenProcess = new QProcess();
            connect(d->lockScreenProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this] {
                // Tell logind that we're not locked
                d->logindInterface->asyncCall("SetLockedHint", false);

                d->lockScreenProcess->deleteLater();
            });
            connect(d->lockScreenProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    // Send a notification
                    auto notification = new tNotification();
                    notification->setSummary(tr("Unable to lock screen"));
                    notification->setText(tr("Sorry, the screen lock was unable to be started."));
                    notification->post();

                    // Tell logind that we're not locked
                    d->logindInterface->asyncCall("SetLockedHint", false);

                    d->lockScreenProcess->deleteLater();
                }
            });

            // "sleep" flag: the locker plays its sunset straight away
            d->lockScreenProcess->start(QStringLiteral(SYSTEM_LIBRARY_DIRECTORY "/td-locker"), flags.contains(QStringLiteral("sleep")) ? QStringList{QStringLiteral("--sleep")} : QStringList()); // Lock Screen

            // Tell logind that we're locked
            d->logindInterface->asyncCall("SetLockedHint", true);
            break;
        case PowerManager::SwitchUsers:
            {
                QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DisplayManager", qEnvironmentVariable("XDG_SEAT_PATH"), "org.freedesktop.DisplayManager.Seat", "SwitchToGreeter");
                QDBusConnection::systemBus().call(message);
                break;
            }
        case PowerManager::TurnOffScreen:
            DesktopWm::setScreenOff(true);
            break;
        case PowerManager::All:
            // Doesn't make sense ???
            break;
    }
}
