#include "sleepmonitor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QTimer>
#include <tsettings.h>

#include <powermanager.h>
#include <statemanager.h>

struct SleepMonitorPrivate {
        tSettings settings;

        // Held while awake so logind/elogind waits for us to lock the screen
        // before it actually suspends.
        QDBusUnixFileDescriptor sleepInhibitor;
        bool acquiring = false;
};

SleepMonitor::SleepMonitor(QObject* parent) :
    QObject{parent} {
    d = new SleepMonitorPrivate();

    QDBusConnection::systemBus().connect("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep", this, SLOT(PrepareForSleep(bool)));
    acquireInhibitor();
}

SleepMonitor::~SleepMonitor() {
    delete d;
}

void SleepMonitor::acquireInhibitor() {
    if (d->sleepInhibitor.isValid() || d->acquiring) return;
    d->acquiring = true;

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Inhibit");
    message.setArguments({QStringLiteral("sleep"), QStringLiteral("theDesk"), tr("Locking the screen before sleeping"), QStringLiteral("delay")});

    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* watcher) {
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher;
        d->acquiring = false;
        if (reply.isValid()) d->sleepInhibitor = reply.value();
        watcher->deleteLater();
    });
}

void SleepMonitor::releaseInhibitor() {
    // Dropping the last copy of the descriptor closes it, which releases the lock
    d->sleepInhibitor = QDBusUnixFileDescriptor();
}

void SleepMonitor::PrepareForSleep(bool sleeping) {
    if (sleeping) {
        if (d->settings.value("Power/suspend.lockScreen").toBool()) {
            // Lock the screen now, then give the locker time to start and play
            // its sunset (~0.8 s) before allowing the system to suspend.
            // logind/elogind allow up to InhibitDelayMaxSec (5 s by default).
            StateManager::powerManager()->performPowerOperation(PowerManager::Lock, {QStringLiteral("sleep")});
            QTimer::singleShot(1600, this, &SleepMonitor::releaseInhibitor);
        } else {
            releaseInhibitor();
        }
    } else {
        // Resumed: take a new inhibitor for the next suspend
        acquireInhibitor();
    }
}
