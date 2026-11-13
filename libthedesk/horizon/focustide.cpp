/****************************************
 *
 *   theDesk - focus tide
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "focustide.h"

#include "hudmanager.h"
#include "quietmodemanager.h"
#include "statemanager.h"

#include <QDBusConnection>
#include <QDateTime>
#include <QTimer>
#include <tsettings.h>

struct FocusTidePrivate {
        QTimer tick;
        QDateTime startedAt;
        qint64 durationMs = 0;
        int held = 0;
        QuietModeManagerTd::QuietMode previousQuietMode = QuietModeManagerTd::Sound;
        tSettings settings;
};

FocusTide* FocusTide::instance() {
    static auto* instance = new FocusTide();
    return instance;
}

FocusTide::FocusTide(QObject* parent) :
    QObject(parent) {
    d = new FocusTidePrivate();
    d->tick.setInterval(1000); // the tide moves once a second; cheap
    connect(&d->tick, &QTimer::timeout, this, [this] {
        double p = progress();
        if (p >= 1) {
            end(true);
        } else {
            emit progressChanged(p);
        }
    });

    // Count notifications that arrive during the tide (broadcast by the
    // Notifications plugin)
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/com/vicr123/theDesk/Notifications"), QStringLiteral("com.vicr123.theDesk.Notifications"), QStringLiteral("Posted"), this, SLOT(notificationPosted(QString,QString,QString)));
}

FocusTide::~FocusTide() {
    delete d;
}

bool FocusTide::isActive() const {
    return d->tick.isActive();
}

double FocusTide::progress() const {
    if (!isActive() || d->durationMs <= 0) return -1;
    return qBound(0.0, double(d->startedAt.msecsTo(QDateTime::currentDateTime())) / d->durationMs, 1.0);
}

void FocusTide::start(int minutes) {
    if (isActive()) return;
    if (minutes <= 0) {
        minutes = d->settings.keysInGroup(QStringLiteral("Focus")).contains(QStringLiteral("minutes")) ? d->settings.value(QStringLiteral("Focus/minutes")).toInt() : 25;
        if (minutes <= 0) minutes = 25;
    }
    d->durationMs = qint64(minutes) * 60 * 1000;
    d->startedAt = QDateTime::currentDateTime();
    d->held = 0;

    // Hold notifications back while the tide is out
    d->previousQuietMode = StateManager::quietModeManager()->currentMode();
    if (d->previousQuietMode == QuietModeManagerTd::Sound) {
        StateManager::quietModeManager()->setQuietMode(QuietModeManagerTd::CriticalOnly);
    }

    d->tick.start();
    StateManager::hudManager()->showHud({
        {"icon", "chronometer"},
        {"title", tr("Focus tide")},
        {"text", minutes == 1 ? tr("1 minute; notifications will wait") : tr("%1 minutes; notifications will wait").arg(minutes)}
    });
    emit started();
    emit progressChanged(0);
}

void FocusTide::stop() {
    if (isActive()) end(false);
}

void FocusTide::toggle() {
    if (isActive()) {
        stop();
    } else {
        start();
    }
}

void FocusTide::end(bool completed) {
    d->tick.stop();
    // Only restore if we changed it and the user hasn't changed it since
    if (d->previousQuietMode == QuietModeManagerTd::Sound && StateManager::quietModeManager()->currentMode() == QuietModeManagerTd::CriticalOnly) {
        StateManager::quietModeManager()->setQuietMode(QuietModeManagerTd::Sound);
    }

    QString text = d->held == 0 ? tr("Nothing waited for you") : d->held == 1 ? tr("1 notification waited for you") : tr("%1 notifications waited for you").arg(d->held);
    StateManager::hudManager()->showHud({
        {"icon", completed ? "chronometer" : "media-playback-stop"},
        {"title", completed ? tr("The tide has turned") : tr("Focus tide stopped")},
        {"text", text}
    });
    emit progressChanged(-1);
    emit finished(d->held);
}

void FocusTide::notificationPosted(QString appName, QString iconName, QString summary) {
    Q_UNUSED(appName)
    Q_UNUSED(iconName)
    Q_UNUSED(summary)
    if (isActive()) d->held++;
}
