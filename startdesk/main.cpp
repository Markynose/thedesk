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

#include "splash/splashcontroller.h"
#include <tapplication.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <m3cursor.h>
#include <memory>
#include <m3scheme.h>
#include "themesync.h"
#include <Screens/screendaemon.h>
#include <tsettings.h>


int main(int argc, char* argv[]) {
    // Put environment variables
    qputenv("QT_QPA_PLATFORMTHEME", "thedesk-platform");

    // Turn off flatpak forwarding for ourselves
    qputenv("TD_PLATFORM_NO_FLATPAK_FORWARD", "1");

    QString oldPath = qgetenv("PATH");
    qputenv("PATH", ("/usr/bin/thedesk-xdg-utils:" + oldPath).toUtf8());

    tApplication a(argc, argv);
    a.setApplicationName("theDesk");
    a.setOrganizationName("theSuite");
    a.setApplicationDisplayName("theDesk");
    a.setQuitOnLastWindowClosed(false);

    qunsetenv("TD_PLATFORM_NO_FLATPAK_FORWARD");

    tSettings::registerDefaults(a.applicationDirPath() + "/defaults.conf");
    tSettings::registerDefaults("/etc/theSuite/theDesk/defaults.conf");

    tSettings* settings = new tSettings();

    // Check for initialisation script
    if (settings->value("Session/UseInitializationScript").toBool()) {
        QProcess process;
        process.start(settings->value("Session/InitializationScript").toString(), QStringList());
        process.waitForFinished();
    }

    // The login keyring: PAM (pam_gnome_keyring) started gnome-keyring and
    // unlocked it with the login password, but it waits for the session to
    // set it up, which GNOME's autostart entries only do in GNOME, Unity,
    // MATE and Cinnamon. Without this nothing took the secrets service, and
    // the first app to ask for a saved password got a fresh, locked keyring
    // (GNOME's "unlock" prompt)
    {
        const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        const QString keyring = QStandardPaths::findExecutable(QStringLiteral("gnome-keyring-daemon"));
        if (!runtime.isEmpty() && !keyring.isEmpty() && QFileInfo::exists(runtime + QStringLiteral("/keyring/control"))) {
            QProcess start;
            start.start(keyring, {QStringLiteral("--start"), QStringLiteral("--components=secrets,pkcs11")});
            if (start.waitForFinished(5000)) {
                // It prints the variables apps use to find it
                for (const QByteArray& line : start.readAllStandardOutput().split('\n')) {
                    const qsizetype equals = line.indexOf('=');
                    if (equals > 0) qputenv(line.left(equals).constData(), line.mid(equals + 1));
                }
            }
        }
    }

    // Export the session environment to D-Bus activated services (portals,
    // polkit agent, etc.). UpdateActivationEnvironment works with any bus
    // daemon, so this also covers OpenRC/runit/s6 systems without systemd.
    QMap<QString, QString> activationEnvironment = {
        {QStringLiteral("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_CURRENT_DESKTOP")},
        {QStringLiteral("QT_QPA_PLATFORMTHEME"), qEnvironmentVariable("QT_QPA_PLATFORMTHEME")},
        {QStringLiteral("PATH"), qEnvironmentVariable("PATH")}
    };
    for (const auto& variable : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_SESSION_TYPE", "XDG_SESSION_DESKTOP", "XCURSOR_THEME", "XCURSOR_SIZE"}) {
        if (qEnvironmentVariableIsSet(variable)) activationEnvironment.insert(QString::fromLatin1(variable), qEnvironmentVariable(variable));
    }

    qDBusRegisterMetaType<QMap<QString, QString>>(); // marshalled as a{ss}
    QDBusMessage updateActivationMessage = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), QStringLiteral("UpdateActivationEnvironment"));
    updateActivationMessage.setArguments({QVariant::fromValue(activationEnvironment)});
    QDBusConnection::sessionBus().call(updateActivationMessage, QDBus::Block, 5000);

    // Under a systemd user manager, also import the environment there and
    // restart the portal so it picks up the new variables.
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.freedesktop.systemd1"))) {
        QStringList systemdEnvironment;
        for (auto i = activationEnvironment.cbegin(); i != activationEnvironment.cend(); i++) {
            systemdEnvironment.append(QStringLiteral("%1=%2").arg(i.key(), i.value()));
        }
        QDBusMessage setEnvironmentMessage = QDBusMessage::createMethodCall("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "SetEnvironment");
        setEnvironmentMessage.setArguments({systemdEnvironment});
        QDBusConnection::sessionBus().asyncCall(setEnvironmentMessage);

        QDBusMessage restartPortalMessage = QDBusMessage::createMethodCall("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "RestartUnit");
        restartPortalMessage.setArguments({QStringLiteral("xdg-desktop-portal.service"), QStringLiteral("replace")});
        QDBusConnection::sessionBus().asyncCall(restartPortalMessage);
    }

    // The theDesk-M3 pointer in this session's M3 colours: written to
    // ~/.local/share/icons (ahead of the neutral copy in /usr/share/icons) so
    // every app started from here on loads it. Rewritten when the horizon
    // shifts the palette.
    // Also kitty's colours and bspwm's borders (ThemeSync), so programs that
    // don't read theDesk's palette still wear it
    {
        auto writeCursors = [] {
            if (!M3Cursor::active()) return;
            M3Cursor::Colours colours = M3Cursor::schemeColours();
            QThread* thread = QThread::create([colours] {
                M3Cursor::writeTheme(M3Cursor::userIconsDir(), colours);
            });
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            thread->start(QThread::LowPriority);
        };
        auto sync = [writeCursors] {
            writeCursors();
            ThemeSync::apply();
        };
        writeCursors();
        // bspwm starts with the session: give it a moment before its borders
        QTimer::singleShot(4000, &a, [] {
            ThemeSync::apply();
        });
        // startdesk runs before theDesk's platform theme is in place, so it
        // never sees palette changes: look once a minute instead, and redo
        // it all when the scheme's hue has moved enough to notice or another
        // colour was chosen (not every minute through a slow blend)
        auto* watch = new QTimer(&a);
        watch->setInterval(60000);
        auto lastSeed = std::make_shared<QColor>(M3::seedFor(M3::Config::fromSettings()));
        QObject::connect(watch, &QTimer::timeout, &a, [sync, lastSeed] {
            const QColor seed = M3::seedFor(M3::Config::fromSettings());
            if (seed.rgb() == lastSeed->rgb()) return;
            if (seed.hslHueF() >= 0 && lastSeed->hslHueF() >= 0) {
                double hueMoved = std::abs(seed.hslHueF() - lastSeed->hslHueF()) * 360;
                if (qMin(hueMoved, 360 - hueMoved) < 8) return;
            }
            *lastSeed = seed;
            sync();
        });
        watch->start();
    }

    SplashController::instance()->startDE();

    return a.exec();
}
