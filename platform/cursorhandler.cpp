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
#include "cursorhandler.h"

#include <QDir>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QLibrary>
#include <QTimer>
#include <QWindow>
#include <tlogger.h>
#include <tsettings.h>


struct CursorHandlerPrivate {
    tSettings* settings;
    QFileSystemWatcher* watcher = nullptr;
    QTimer* reapply = nullptr;
};

CursorHandler::CursorHandler(QObject* parent) : QObject(parent) {
    d = new CursorHandlerPrivate();
    d->settings = new tSettings("theSuite", "theDesk.platform", this);

    connect(d->settings, &tSettings::settingChanged, this, [this](QString key, QVariant value) {
        if (key == "Platform/cursor" || key == "Platform/cursorSize") {
            this->updateApplicationCursor();
        }
    });
    this->updateApplicationCursor();

    // The pointer theme can be rewritten in place (theDesk's M3 pointer
    // follows the horizon's colours). The session announces it over
    // XSETTINGS, which makes Qt drop its cursor cache; then put the pointer
    // back on every window (Qt sets one even for the plain arrow), which
    // otherwise keeps the old one until its shape changes. After the
    // announcement, so the new files are the ones loaded.
    d->reapply = new QTimer(this);
    d->reapply->setSingleShot(true);
    d->reapply->setInterval(2000);
    connect(d->reapply, &QTimer::timeout, this, [] {
        for (QWindow* window : QGuiApplication::allWindows()) {
            if (!window->handle() || !window->isVisible()) continue;
            const QCursor cursor = window->cursor();
            window->unsetCursor();
            window->setCursor(cursor);
        }
    });
    // The platform theme is made before the application's event dispatcher
    // exists, and a watcher made now would never report: set it up once the
    // event loop is up
    QMetaObject::invokeMethod(this, [this] {
        d->watcher = new QFileSystemWatcher(this);
        QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
        if (dataHome.isEmpty()) dataHome = QDir::homePath() + QStringLiteral("/.local/share");
        const QString iconsDir = dataHome + QStringLiteral("/icons");
        if (QFileInfo(iconsDir).isDir()) d->watcher->addPath(iconsDir);
        connect(d->watcher, &QFileSystemWatcher::directoryChanged, d->reapply, qOverload<>(&QTimer::start));
    }, Qt::QueuedConnection);
}

CursorHandler::~CursorHandler() {
    delete d;
}

void CursorHandler::updateApplicationCursor() {
#if QT_CONFIG(xcb)
    auto* x11App = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    if (!x11App || !x11App->display()) return; // Not running on X11

    QString theme = d->settings->value("Platform/cursor").toString();
    int size = d->settings->value("Platform/cursorSize").toInt();

    // Load libXcursor at runtime (versioned soname, so no -dev symlink is needed)
    static QLibrary xc(QStringLiteral("Xcursor"), 1);
    if (!xc.isLoaded() && !xc.load()) {
        tDebug("CursorHandler") << "Could not load XCursor library: " + xc.errorString();
        return;
    }

    typedef int (*setThemeFunc)(Display*, const char*);
    typedef int (*setSizeFunc)(Display*, int);

    auto XcursorSetTheme = reinterpret_cast<setThemeFunc>(xc.resolve("XcursorSetTheme"));
    auto XcursorSetDefaultSize = reinterpret_cast<setSizeFunc>(xc.resolve("XcursorSetDefaultSize"));
    if (!XcursorSetTheme || !XcursorSetDefaultSize) {
        tDebug("CursorHandler") << "XCursor library is missing XcursorSetTheme/XcursorSetDefaultSize";
        return;
    }

    // Set cursors
    if (!theme.isEmpty()) XcursorSetTheme(x11App->display(), qPrintable(theme));
    if (size > 0) XcursorSetDefaultSize(x11App->display(), size);

    // Child processes (and Qt's own cursor loading) honour these as well
    if (!theme.isEmpty()) qputenv("XCURSOR_THEME", theme.toUtf8());
    if (size > 0) qputenv("XCURSOR_SIZE", QByteArray::number(size));
#endif
}
