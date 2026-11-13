/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "libcontemporary_global.h"
#include <QStyle>

#include "tsettings.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>

#include "private/nativeeventfilter.h"

#ifdef QT_WIDGETS_LIB
    #include <QPainter>
    #include <QWidget>
    #include <QWindow>
#endif

#ifdef Q_OS_WIN
    #include <Windows.h>
#endif

#ifdef HAVE_QT_DBUS
    #include <QDBusInterface>
#endif

struct libContemporaryCommonPrivate {
        bool powerStretch = false;
        libContemporaryPrivate::NativeEventFilter* filter;
#ifdef T_OS_UNIX_NOT_MAC
        QSettings* themeSettings = new QSettings("theSuite", "ts-qtplatform");
        tSettings* contemporarySettings;
#endif
};

libContemporaryCommon::libContemporaryCommon() :
    QObject(nullptr) {
    d = new libContemporaryCommonPrivate();

    // Install the native event filter
    d->filter = libContemporaryPrivate::NativeEventFilter::instance(this);
    QApplication::instance()->installNativeEventFilter(d->filter);
    connect(d->filter, &libContemporaryPrivate::NativeEventFilter::powerStretchChanged, this, &libContemporaryCommon::powerStretchChangedPrivate);

#ifdef T_OS_UNIX_NOT_MAC
    d->contemporarySettings = new tSettings("theSuite", "contemporary_widget", this);

    if (qEnvironmentVariable("XDG_CURRENT_DESKTOP") == "theshell") {
        QDBusMessage message = QDBusMessage::createMethodCall("org.thesuite.theshell", "/org/thesuite/Power", "org.thesuite.Power", "powerStretch");
        QDBusReply<bool> reply = QDBusConnection::sessionBus().call(message);
        if (reply.isValid()) {
            d->powerStretch = reply.value();
        }

        QDBusConnection::sessionBus().connect("org.thesuite.theshell", "/org/thesuite/Power", "org.thesuite.Power", "powerStretchChanged", this, SIGNAL(powerStretchChangedPrivate(bool)));
    } else {
        QDBusInterface powerProfilesDaemon("net.hadess.PowerProfiles", "/net/hadess/PowerProfiles", "net.hadess.PowerProfiles", QDBusConnection::systemBus());

        QString currentProfile = powerProfilesDaemon.property("ActiveProfile").toString();
        d->powerStretch = currentProfile == "power-saver";

        QDBusConnection::systemBus().connect("net.hadess.PowerProfiles", "/net/hadess/PowerProfiles", "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(dbusPropertyChangedPrivate(QString, QMap<QString, QVariant>, QStringList)));
    }
#elif defined(Q_OS_WIN)
    // Register for power notifications
    QWidget* powerNotificationHandleWidget = new QWidget();
    RegisterPowerSettingNotification(HWND(powerNotificationHandleWidget->winId()), &GUID_POWER_SAVING_STATUS, 0);

    // Query power saver
    SYSTEM_POWER_STATUS powerStatus;
    BOOL success = GetSystemPowerStatus(&powerStatus);
    if (success) {
        d->powerStretch = powerStatus.SystemStatusFlag;
    }

#endif
}

libContemporaryCommon* libContemporaryCommon::instance() {
    static libContemporaryCommon* appInst = new libContemporaryCommon;
    return appInst;
}

bool libContemporaryCommon::powerStretchEnabled() {
    return d->powerStretch;
}

void libContemporaryCommon::powerStretchChangedPrivate(bool isOn) {
    d->powerStretch = isOn;

    emit powerStretchChanged(isOn);
}

void libContemporaryCommon::dbusPropertyChangedPrivate(QString interfaceName, QMap<QString, QVariant> changedProperties, QStringList invalidatedProperties) {
    if (interfaceName == "net.hadess.PowerProfiles" && changedProperties.contains("ActiveProfile")) {
        d->powerStretch = changedProperties.value("ActiveProfile").toString() == "power-saver";
        emit powerStretchChanged(d->powerStretch);
    }
}

bool libContemporaryCommon::allowSystemAnimations() {
#ifdef T_OS_UNIX_NOT_MAC
    return d->themeSettings->value("accessibility/systemAnimations", true).toBool();
#else
    return true;
#endif
}

#ifdef QT_WIDGETS_LIB
    #include <QScreen>
double libContemporaryCommon::getDPIScaling() {
    #if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    return 1;
    #else
        //    return QApplication::primaryScreen()->devicePixelRatio();
    #endif
    return 1;
}
#endif

QStringList libContemporaryCommon::searchInPath(QString executable) {
    QStringList executables;
    QStringList pathDirs = QString(qgetenv("PATH")).split(":");
    for (QString dir : pathDirs) {
        QDir path(dir);
        QDirIterator i(path);
        while (i.hasNext()) {
            i.next();
            if (i.fileName() == executable) {
                executables.append(i.filePath());
            }
        }
    }

    return executables;
}

bool libContemporaryCommon::isMaterial3() {
    return qApp && qApp->style() && qApp->style()->objectName().compare(QStringLiteral("Material3"), Qt::CaseInsensitive) == 0;
}

QColor libContemporaryCommon::lineColor(QColor textColor) {
    // theDesk M3: dividers are faint (OutlineVariant-like), not full-strength text
    if (isMaterial3()) {
        textColor.setAlphaF(0.16);
        return textColor;
    }
#ifdef T_OS_UNIX_NOT_MAC
    if (instance()->d->contemporarySettings->value("Lines/reduceIntensity").toBool()) {
        textColor.setAlpha(127);
    }
#endif
    return textColor;
}

QString libContemporaryCommon::humanReadablePartJoinString() {
    return QStringLiteral(" · ");
}

#ifdef QT_WIDGETS_LIB
void libContemporaryCommon::tintImage(QImage& image, QColor tint) {
    // bool doPaint = true;
    int failNum = 0;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            QColor pixelCol = image.pixelColor(x, y);
            // int blue = pixelCol.blue(), green = pixelCol.green(), red = pixelCol.red();
            if ((pixelCol.blue() > pixelCol.green() - 10 && pixelCol.blue() < pixelCol.green() + 10) &&
                (pixelCol.green() > pixelCol.red() - 10 && pixelCol.green() < pixelCol.red() + 10)) {
            } else {
                failNum++;
                // doPaint = false;
            }
        }
    }

    if (failNum < (image.size().width() * image.size().height()) / 8) {
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        painter.fillRect(0, 0, image.width(), image.height(), tint);
        painter.end();
    }
}

QPixmap libContemporaryCommon::getTintedPixmap(QPixmap pixmap, QColor tint) {
    auto image = pixmap.toImage();
    tintImage(image, tint);
    return QPixmap::fromImage(image);
}

double libContemporaryCommon::getDPIScaling(const QPaintDevice* paintDevice) {
    #if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    return 1;
    #endif
    //    if (!paintDevice) return QApplication::primaryScreen()->devicePixelRatio();
    //    return paintDevice->devicePixelRatio();
    return 1;
}

void libContemporaryCommon::fixateHeight(QWidget* widget, std::function<int()> calculateHeight) {
    connect(widget->window()->windowHandle(), &QWindow::screenChanged, widget, [=] {
        widget->setFixedHeight(calculateHeight());
    });
    widget->setFixedHeight(calculateHeight());
}

#endif
