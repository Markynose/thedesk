/****************************************
 *
 *   theDesk - the screen's backlight
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "backlight.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include <unistd.h>

namespace {
    SystemScreen* randrScreen() {
        SystemScreen* screen = ScreenDaemon::instance()->primayScreen();
        return screen && screen->isScreenBrightnessAvailable() ? screen : nullptr;
    }

    QString readFile(const QString& path) {
        QFile file(path);
        return file.open(QFile::ReadOnly) ? QString::fromLatin1(file.readAll()).trimmed() : QString();
    }

    // The kernel's backlight: firmware first, then platform, then raw (the
    // GPU's own, e.g. intel_backlight)
    QString device() {
        static QString chosen;
        static bool looked = false;
        if (looked) return chosen;
        looked = true;
        const QDir dir(QStringLiteral("/sys/class/backlight"));
        int best = -1;
        for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString type = readFile(dir.filePath(name + QStringLiteral("/type")));
            const int rank = type == QStringLiteral("firmware") ? 3 : type == QStringLiteral("platform") ? 2 : 1;
            if (rank > best) {
                best = rank;
                chosen = name;
            }
        }
        return chosen;
    }

    int maximum() {
        return readFile(QStringLiteral("/sys/class/backlight/%1/max_brightness").arg(device())).toInt();
    }

    QString sessionPath() {
        static QString path;
        if (!path.isEmpty()) return path;
        QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("GetSessionByPID"));
        call.setArguments({uint(getpid())});
        QDBusReply<QDBusObjectPath> reply = QDBusConnection::systemBus().call(call, QDBus::Block, 2000);
        if (reply.isValid()) path = reply.value().path();
        return path;
    }
} // namespace

bool Backlight::available() {
    if (randrScreen()) return true;
    return !device().isEmpty() && maximum() > 0 && !sessionPath().isEmpty();
}

double Backlight::value() {
    if (SystemScreen* screen = randrScreen()) return screen->screenBrightness();
    const int max = maximum();
    if (max <= 0) return 0;
    return readFile(QStringLiteral("/sys/class/backlight/%1/brightness").arg(device())).toInt() / double(max);
}

void Backlight::set(double value) {
    value = qBound(0.0, value, 1.0);
    if (SystemScreen* screen = randrScreen()) return screen->setScreenBrightness(value);
    const int max = maximum();
    if (max <= 0 || sessionPath().isEmpty()) return;
    // Never fully dark: 0 switches some panels' backlight off entirely
    const uint level = uint(qMax(1L, qRound64(value * max)));
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), sessionPath(), QStringLiteral("org.freedesktop.login1.Session"), QStringLiteral("SetBrightness"));
    call.setArguments({QStringLiteral("backlight"), device(), level});
    QDBusConnection::systemBus().asyncCall(call);
}
