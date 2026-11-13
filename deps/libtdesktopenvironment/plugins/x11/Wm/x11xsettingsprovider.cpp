#include "x11xsettingsprovider.h"

#include "Screens/screendaemon.h"
#include <QColor>
#include <QDir>
#include <QFileSystemWatcher>
#include <QFontInfo>
#include <QGuiApplication>
#include <QLibrary>
#include <QMap>
#include <QTimer>
#include <tx11info.h>

#include "x11functions.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>

template<typename T> QByteArray stringToXSettingProperty(QString string) {
    QByteArray data;
    QByteArray utf8String = string.toUtf8();

    T stringLength = utf8String.length();

    data.append(reinterpret_cast<char*>(&stringLength), sizeof(stringLength));
    data.append(utf8String);

    // Pad out the rest with bytes
    int pad = (4 - (stringLength % 4)) % 4;
    data.append(pad, 0);

    return data;
}

struct XSetting {
        enum SettingType : char {
            Integer = 0,
            String = 1,
            Colour = 2
        };

        SettingType type;
        QString name;
        quint32 lastChangeSerial;

        quint32 intValue;
        QString stringValue;
        QColor colorValue;

        QByteArray exportToProperty() {
            QByteArray data;

            data.append(type);
            data.append(static_cast<char>(0)); // -
            data.append(stringToXSettingProperty<quint16>(name));
            data.append(reinterpret_cast<char*>(&lastChangeSerial), sizeof(lastChangeSerial));

            switch (type) {
                case XSetting::Integer:
                    data.append(reinterpret_cast<char*>(&intValue), sizeof(intValue));
                    break;
                case XSetting::String:
                    data.append(stringToXSettingProperty<quint32>(stringValue));
                    break;
                case XSetting::Colour:
                    quint16 r, g, b, a;
                    r = colorValue.red();
                    g = colorValue.green();
                    b = colorValue.blue();
                    a = colorValue.alpha();

                    data.append(reinterpret_cast<char*>(&r), sizeof(r));
                    data.append(reinterpret_cast<char*>(&g), sizeof(g));
                    data.append(reinterpret_cast<char*>(&b), sizeof(b));
                    data.append(reinterpret_cast<char*>(&a), sizeof(a));
                    break;
            }

            return data;
        }
};

struct X11XSettingsProviderPrivate {
        Window settingsWindow = 0;
        quint32 serial = 0;

        QMap<QString, XSetting> settings;

        QString cursorTheme;
        QFileSystemWatcher* cursorWatcher = nullptr;
        QTimer* cursorChanged = nullptr;
};

namespace {
    // The pointer theme and size the session uses (Xcursor.theme/size in the
    // X resources, else XCURSOR_THEME/XCURSOR_SIZE)
    QString sessionCursorTheme() {
        const char* resource = XGetDefault(tX11Info::display(), "Xcursor", "theme");
        if (resource && *resource) return QString::fromUtf8(resource);
        const QString env = qEnvironmentVariable("XCURSOR_THEME");
        return env.isEmpty() ? QStringLiteral("default") : env;
    }

    int sessionCursorSize() {
        const char* resource = XGetDefault(tX11Info::display(), "Xcursor", "size");
        int size = resource ? QByteArray(resource).toInt() : 0;
        if (size <= 0) size = qEnvironmentVariableIntValue("XCURSOR_SIZE");
        return size > 0 ? size : 24;
    }

    // The root window's pointer, from the theme as it is on disk now. Most
    // windows don't set a pointer of their own and show the root's, which
    // the window manager loaded once when the session started.
    void reloadRootCursor() {
        static QLibrary xcursor(QStringLiteral("Xcursor"), 1);
        if (!xcursor.isLoaded() && !xcursor.load()) return;
        using LoadCursor = Cursor (*)(Display*, const char*);
        auto load = reinterpret_cast<LoadCursor>(xcursor.resolve("XcursorLibraryLoadCursor"));
        if (!load) return;
        Display* display = tX11Info::display();
        Cursor cursor = load(display, "left_ptr");
        if (!cursor) cursor = load(display, "default");
        if (!cursor) return;
        XDefineCursor(display, tX11Info::appRootWindow(), cursor);
        XFreeCursor(display, cursor);
        XFlush(display);
    }

    // "Family size", as GTK reads Gtk/FontName
    QString applicationFontName() {
        const QFont font = QGuiApplication::font();
        const double points = font.pointSizeF() > 0 ? font.pointSizeF() : 10;
        return QStringLiteral("%1 %2").arg(QFontInfo(font).family()).arg(points);
    }
} // namespace

X11XSettingsProvider::X11XSettingsProvider(QObject* parent) :
    QObject{parent} {
    d = new X11XSettingsProviderPrivate();

    d->cursorTheme = sessionCursorTheme();
    setString("Gtk/CursorThemeName", d->cursorTheme);
    setInt("Gtk/CursorThemeSize", sessionCursorSize());
    setString("Gtk/FontName", applicationFontName());
    qGuiApp->installEventFilter(this);

    // A pointer theme can be rewritten while the session runs (theDesk's M3
    // pointer follows the horizon's colours). X clients keep the cursors they
    // loaded, so when the theme's files change, put the new arrow on the root
    // window (which most windows show) and announce the theme again: the new
    // serial makes Qt apps drop their cursor cache and load the new files the
    // next time they set a pointer.
    d->cursorChanged = new QTimer(this);
    d->cursorChanged->setSingleShot(true);
    d->cursorChanged->setInterval(750);
    connect(d->cursorChanged, &QTimer::timeout, this, [this] {
        reloadRootCursor();
        if (d->settings.value("Gtk/CursorThemeName").stringValue == d->cursorTheme) setString("Gtk/CursorThemeName", d->cursorTheme);
    });
    d->cursorWatcher = new QFileSystemWatcher(this);
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty()) dataHome = QDir::homePath() + QStringLiteral("/.local/share");
    for (const QString& iconsDir : {dataHome + QStringLiteral("/icons"), QDir::homePath() + QStringLiteral("/.icons")}) {
        if (QFileInfo(iconsDir).isDir()) d->cursorWatcher->addPath(iconsDir);
    }
    connect(d->cursorWatcher, &QFileSystemWatcher::directoryChanged, d->cursorChanged, qOverload<>(&QTimer::start));

    connect(ScreenDaemon::instance(), &ScreenDaemon::dpiChanged, this, [this] {
        setInt("Xft/DPI", ScreenDaemon::instance()->dpi() * 1024);
    });
    setInt("Xft/DPI", ScreenDaemon::instance()->dpi() * 1024);
}

bool X11XSettingsProvider::eventFilter(QObject* watched, QEvent* event) {
    if (watched == qGuiApp && event->type() == QEvent::ApplicationFontChange) {
        setString("Gtk/FontName", applicationFontName());
    }
    return QObject::eventFilter(watched, event);
}

X11XSettingsProvider::~X11XSettingsProvider() {
    XDestroyWindow(tX11Info::display(), d->settingsWindow);
    delete d;
}

void X11XSettingsProvider::setString(QString name, QString value) {
    d->serial++;

    XSetting setting;
    setting.type = XSetting::String;
    setting.name = name;
    setting.lastChangeSerial = d->serial;
    setting.stringValue = value;
    d->settings.insert(name, setting);

    updateSetting();
}

void X11XSettingsProvider::setInt(QString name, quint32 value) {
    d->serial++;

    XSetting setting;
    setting.type = XSetting::Integer;
    setting.name = name;
    setting.lastChangeSerial = d->serial;
    setting.intValue = value;
    d->settings.insert(name, setting);

    updateSetting();
}

void X11XSettingsProvider::setColor(QString name, QColor value) {
    d->serial++;

    XSetting setting;
    setting.type = XSetting::Colour;
    setting.name = name;
    setting.lastChangeSerial = d->serial;
    setting.colorValue = value;
    d->settings.insert(name, setting);

    updateSetting();
}

void X11XSettingsProvider::setAsSettingsManager() {
    Atom xsettingsAtom = XInternAtom(tX11Info::display(), "_XSETTINGS_S0", true);

    d->settingsWindow = XCreateSimpleWindow(tX11Info::display(), tX11Info::appRootWindow(), 0, 0, 1, 1, 1, 1, 1);
    XSetSelectionOwner(tX11Info::display(), xsettingsAtom, d->settingsWindow, CurrentTime);

    TX11::sendMessageToRootWindow("MANAGER", d->settingsWindow, CurrentTime, xsettingsAtom, d->settingsWindow);
    updateSetting();
}

void X11XSettingsProvider::updateSetting() {
    if (!d->settingsWindow) return;

    QByteArray propertyValue;

    propertyValue.append(static_cast<char>(0)); // byte-order
    propertyValue.append(static_cast<char>(0)); // -
    propertyValue.append(static_cast<char>(0)); // -
    propertyValue.append(static_cast<char>(0)); // -

    quint32 settingsLength = d->settings.size();
    propertyValue.append(reinterpret_cast<char*>(&d->serial), sizeof(d->serial));
    propertyValue.append(reinterpret_cast<char*>(&settingsLength), sizeof(settingsLength));

    for (XSetting setting : d->settings.values()) {
        propertyValue.append(setting.exportToProperty());
    }

    Atom property = XInternAtom(tX11Info::display(), "_XSETTINGS_SETTINGS", true);
    XChangeProperty(tX11Info::display(), d->settingsWindow, property, property, 8, PropModeReplace, reinterpret_cast<const unsigned char*>(propertyValue.constData()), propertyValue.length());
}
