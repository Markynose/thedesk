/****************************************
 *
 *   theDesk - LightDM greeter
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/

// The login screen: theDesk's M3LockSurface driven by LightDM.
//   thedesk-greeter            run by LightDM (greeter-session=thedesk-greeter)
//   thedesk-greeter --preview  the screen without LightDM, for testing; any
//                              password is "wrong" except "go"
//
// On the first start after boot it opens from Plymouth's last frame; after
// a log-out it just opens. A successful login plays the exit into theDesk's
// loading screen and leaves the hand-off on the root window before the
// session starts.

#include <QActionGroup>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include "lightdmclient.h"
#include <m3locksurface.h>
#include <m3scheme.h>
#include <tsettings.h>
#include <QFileInfo>
#include <sys/stat.h>

namespace {
    // Plymouth's frame is only there on the first start after boot
    bool firstStartSinceBoot() {
        const QString marker = QStringLiteral("/tmp/.thedesk-greeter-started");
        const bool first = !QFile::exists(marker);
        QFile file(marker);
        if (file.open(QFile::WriteOnly)) file.close();
        return first;
    }

    QToolButton* actionButton(const QString& icon, const QString& tooltip) {
        auto* button = new QToolButton();
        button->setIcon(QIcon::fromTheme(icon));
        button->setIconSize(QSize(24, 24));
        button->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
        button->setPopupMode(QToolButton::InstantPopup);
        button->setToolTip(tooltip);
        return button;
    }
} // namespace

int main(int argc, char* argv[]) {
    // The greeter runs as the lightdm user, outside any theDesk session
    qputenv("QT_QPA_PLATFORMTHEME", "thedesk-platform");
    qputenv("XCURSOR_THEME", "theDesk-M3");
    qunsetenv("QT_STYLE_OVERRIDE");
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("thedesk-greeter"));
    app.setOrganizationName(QStringLiteral("theSuite"));
    app.setStyle(QStringLiteral("Material3"));
    app.setPalette(M3::currentScheme().toPalette());
    QFont font(QStringLiteral("Roboto Flex"));
    font.setPixelSize(14);
    app.setFont(font);
    const bool preview = app.arguments().contains(QStringLiteral("--preview"));

    LightDMClient greeter;
    if (!preview && !greeter.connectToDaemon()) {
        qWarning("thedesk-greeter: can't connect to LightDM");
        return 1;
    }

    // Who logs in: LightDM's choice, else the first user
    QString userName, displayName, picture, userSession;
    const QString hint = preview ? QString() : greeter.selectUserHint();
    for (const LightDMClient::User& user : greeter.users()) {
        if (!userName.isEmpty() && user.name != hint) continue;
        userName = user.name;
        displayName = user.displayName;
        picture = user.image;
        userSession = user.session;
        if (user.name == hint) break;
    }
    if (preview && userName.isEmpty()) userName = QStringLiteral("preview");
    if (displayName.isEmpty()) displayName = userName;
    if (!QFile::exists(picture)) picture.clear();

    // The user's colour choice, left by their session (ThemeSync), so this
    // screen and theDesk's loading screen wear the same colours
    QString sharedDir;
    if (!preview && !userName.isEmpty()) sharedDir = greeter.sharedDataDir(userName);
    if (!sharedDir.isEmpty()) {
        // LightDM makes its data folder 0770 again when it starts; the user's
        // session needs to pass through it to leave its colours, picture and
        // shape in its own folder. The greeter runs as its owner.
        const QByteArray parent = QFileInfo(sharedDir).absolutePath().toLocal8Bit();
        struct stat info;
        if (::stat(parent.constData(), &info) == 0 && (info.st_mode & 07777) != 01771) ::chmod(parent.constData(), 01771);
    }
    if (!sharedDir.isEmpty()) {
        const QString dir = sharedDir;
        QSettings userPalette(dir + QStringLiteral("/thedesk-palette.conf"), QSettings::IniFormat);
        if (!dir.isEmpty() && userPalette.contains(QStringLiteral("Palette/m3.seed"))) {
            tSettings own(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
            for (const QString& key : userPalette.allKeys()) {
                if (key.startsWith(QStringLiteral("Palette/"))) own.setValue(key, userPalette.value(key));
            }
            app.setPalette(M3::currentScheme().toPalette());
        }
        // The picture's shape (M3UserCard reads UserCard/shapes once, when
        // the first card is built, below); unset there means the defaults
        tSettings desk(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
        if (userPalette.contains(QStringLiteral("UserCard/shapes"))) desk.setValue(QStringLiteral("UserCard/shapes"), userPalette.value(QStringLiteral("UserCard/shapes")));
        else if (desk.keysInGroup(QStringLiteral("UserCard")).contains(QStringLiteral("shapes"))) desk.setValue(QStringLiteral("UserCard/shapes"), QString()); // empty: the defaults
    }

    // The picture: the user's home is usually private to them, so their
    // session leaves a copy of it here too
    if ((picture.isEmpty() || QImage(picture).isNull()) && !sharedDir.isEmpty() && !QImage(sharedDir + QStringLiteral("/picture.png")).isNull()) {
        picture = sharedDir + QStringLiteral("/picture.png");
    }

    M3LockSurface surface;
    surface.setWindowFlag(Qt::FramelessWindowHint);
    surface.setUser(displayName, picture);
    surface.setGreeting(QString()); // no "Good afternoon": the picture and the name

    // Session: the user's last one, else theDesk, else LightDM's default
    QString session = userSession;
    auto* sessionButton = new QToolButton();
    sessionButton->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    sessionButton->setPopupMode(QToolButton::InstantPopup);
    sessionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* sessionMenu = new QMenu(sessionButton);
    auto* sessionGroup = new QActionGroup(sessionMenu);
    QStringList keys;
    for (const LightDMClient::Session& entry : greeter.sessions()) {
        keys.append(entry.key);
        QAction* action = sessionMenu->addAction(entry.name);
        action->setCheckable(true);
        action->setData(entry.key);
        sessionGroup->addAction(action);
    }
    if (!keys.contains(session)) session = keys.contains(QStringLiteral("thedesk")) ? QStringLiteral("thedesk") : greeter.defaultSessionHint();
    auto showSession = [&] {
        for (QAction* action : sessionGroup->actions()) {
            if (action->data().toString() == session) {
                action->setChecked(true);
                sessionButton->setText(action->text());
            }
        }
    };
    showSession();
    QObject::connect(sessionGroup, &QActionGroup::triggered, sessionButton, [&](QAction* action) {
        session = action->data().toString();
        showSession();
    });
    sessionButton->setMenu(sessionMenu);
    // (added right to left: power, then the session chip beside it)
    const bool showSessions = keys.size() > 1;

    // Power
    QToolButton* powerButton = actionButton(QStringLiteral("system-shutdown"), QObject::tr("Power"));
    auto* powerMenu = new QMenu(powerButton);
    if (preview || greeter.canSuspend()) powerMenu->addAction(QIcon::fromTheme(QStringLiteral("system-suspend")), QObject::tr("Sleep"), [&] { if (!preview) greeter.suspend(); });
    if (preview || greeter.canRestart()) powerMenu->addAction(QIcon::fromTheme(QStringLiteral("system-reboot")), QObject::tr("Restart"), [&] { if (!preview) greeter.restart(); });
    if (preview || greeter.canShutdown()) powerMenu->addAction(QIcon::fromTheme(QStringLiteral("system-shutdown")), QObject::tr("Power off"), [&] { if (!preview) greeter.shutdown(); });
    powerButton->setMenu(powerMenu);
    surface.addAction(powerButton);
    if (showSessions) surface.addAction(sessionButton);

    // Authentication: PAM asks, the field answers
    bool prompted = false;
    auto authenticate = [&] {
        prompted = false;
        if (!preview) greeter.authenticate(userName);
        else prompted = true;
    };
    QObject::connect(&greeter, &LightDMClient::showPrompt, &surface, [&](QString text, bool secret) {
        Q_UNUSED(secret)
        prompted = true;
        // PAM says "Password: "; the field already says what it wants
        if (!text.trimmed().startsWith(QStringLiteral("Password"), Qt::CaseInsensitive)) surface.setPlaceholder(text.trimmed().remove(QRegularExpression(QStringLiteral(":$"))));
        surface.setBusy(false);
    });
    QObject::connect(&greeter, &LightDMClient::showMessage, &surface, [&](QString text, bool error) {
        if (error) surface.showError(text);
        else surface.showMessage(text);
    });
    auto succeeded = [&] {
        surface.playExitToSplash();
    };
    QObject::connect(&greeter, &LightDMClient::authenticationComplete, &surface, [&] {
        if (greeter.isAuthenticated()) {
            succeeded();
        } else {
            surface.clearPassword();
            surface.setBusy(false);
            surface.showError(QObject::tr("That password didn't work. Try again."));
            authenticate();
        }
    });
    QObject::connect(&surface, &M3LockSurface::submitted, &surface, [&](QString password) {
        if (!prompted) return;
        prompted = false;
        surface.setBusy(true);
        surface.showMessage(QString());
        if (preview) {
            QTimer::singleShot(400, &surface, [&, password] {
                if (password == QStringLiteral("go")) {
                    succeeded();
                } else {
                    surface.clearPassword();
                    surface.setBusy(false);
                    surface.showError(QObject::tr("That password didn't work. Try again."));
                    prompted = true;
                }
            });
        } else {
            greeter.respond(password);
        }
    });
    QObject::connect(&surface, &M3LockSurface::exitFinished, &surface, [&] {
        // Leave the hand-off for the loading screen, then start the session
        M3LockSurface::writeSplashHandoff(&surface, surface.indicatorElapsed());
        if (preview) {
            QTimer::singleShot(800, &app, &QApplication::quit);
            return;
        }
        if (!greeter.startSession(session)) {
            surface.showError(QObject::tr("The session couldn't start."));
            surface.open(M3LockSurface::Opening::Plain);
            authenticate();
        }
    });

    // On the primary screen, over everything (there's no window manager here)
    QScreen* screen = app.primaryScreen();
    surface.setGeometry(screen->geometry());
    surface.showFullScreen();
    surface.activateWindow();
    surface.open(!preview && firstStartSinceBoot() ? M3LockSurface::Opening::FromBoot
                 : preview && app.arguments().contains(QStringLiteral("--boot")) ? M3LockSurface::Opening::FromBoot
                                                                                   : M3LockSurface::Opening::Plain);
    authenticate();

    // Test hook: THEDESK_GREETER_SHOTS=<dir> with --preview saves frames of
    // the opening, logs in, and saves frames of the exit
    const QString shots = qEnvironmentVariable("THEDESK_GREETER_SHOTS");
    if (preview && !shots.isEmpty()) {
        int n = 0;
        for (int at : {60, 700, 1100, 2000}) QTimer::singleShot(at, &surface, [&, at] { Q_UNUSED(at) surface.grab().save(QStringLiteral("%1/greeter-%2.png").arg(shots).arg(n++)); });
        QTimer::singleShot(2300, &surface, [&] { emit surface.submitted(QStringLiteral("nope")); });
        QTimer::singleShot(2850, &surface, [&] { surface.grab().save(QStringLiteral("%1/greeter-%2.png").arg(shots).arg(n++)); });
        QTimer::singleShot(3400, &surface, [&] { emit surface.submitted(QStringLiteral("go")); });
        for (int at : {4000, 4400, 5000}) QTimer::singleShot(at, &surface, [&, at] { Q_UNUSED(at) surface.grab().save(QStringLiteral("%1/greeter-%2.png").arg(shots).arg(n++)); });
    }
    return app.exec();
}
