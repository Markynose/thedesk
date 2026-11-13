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

#ifndef TAPPLICATION_H
#define TAPPLICATION_H

#include "libcontemporary_global.h"
#include <QApplication>
#include <QJsonObject>
#include <QUrl>

struct tApplicationPrivate;

class LIBCONTEMPORARY_EXPORT tApplication : public QApplication {
        Q_OBJECT
    public:
        explicit tApplication(int& argc, char** argv);
        ~tApplication();

        enum KnownLicenses {
            Gpl3,
            Gpl3OrLater,
            Gpl2,
            Gpl2OrLater,
            Lgpl3,
            Lgpl3OrLater,
            Lgpl2_1,
            Lgpl2_1OrLater,
            Other
        };

        enum UrlType {
            HelpContents,
            Sources,
            FileBug,
            Website
        };

        enum Platform {
            TheDesk,
            Flatpak,
            Windows,
            WindowsAppPackage,
            MacOS,
            OtherPlatform
        };
        Q_DECLARE_FLAGS(Platforms, Platform)

        static QIcon applicationIcon();
        static QStringList shareDirs();
        static QStringList systemShareDirs();
        static QString genericName();
        [[deprecated]] static QPixmap aboutDialogSplashGraphic();
        static QList<QPair<QString, QString>> versions();
        static QStringList copyrightLines();
        static QString copyrightHolder();
        static QString copyrightYear();
        static bool haveApplicationUrl(UrlType type);
        static QUrl applicationUrl(UrlType type);
        static KnownLicenses applicationLicense();
        static void ensureSingleInstance(QJsonObject launchData);
        static Platform currentPlatform();
        static bool isInitialised();

    Q_SIGNALS:
        void openFile(QString file);
        void singleInstanceMessage(QJsonObject launchData);
        void dockIconClicked();
        void updateTranslators();

    public Q_SLOTS:
        void registerCrashTrap();
        static QStringList exportBacktrace();

        static void setApplicationIcon(QIcon icon);
        static void setApplicationShareDir(QString shareDir);
        static void setGenericName(QString genericName);
        [[deprecated]] static void setAboutDialogSplashGraphic(QPixmap aboutDialogSplashGraphic);
        static void addLibraryVersion(QString libraryName, QString version);
        static void addCopyrightLine(QString copyrightLine);
        static void setCopyrightHolder(QString copyrightHolder);
        static void setCopyrightYear(QString copyrightYear);
        static void setApplicationLicense(KnownLicenses license);
        static void setApplicationUrl(UrlType type, QUrl url);

#ifdef Q_OS_WIN
        static void setWinApplicationClassId(QString classId);
#endif

        static QPixmap aboutDialogSplashGraphicFromSvg(QString svgFile);

        void installTranslators();
        static void addPluginTranslator(QString pluginName);
        static void removePluginTranslator(QString pluginName);
        static void addLibraryTranslator(QString sharePath);
        static QString macOSBundlePath();
        static QString macOSBundlePath(QString bundleIdentifier);

        static void restart();

    protected:
        friend tApplicationPrivate;
        static QStringList exportBacktrace(void* data);

    private:
        bool event(QEvent* event);

        static tApplicationPrivate* d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(tApplication::Platforms)

#endif // TAPPLICATION_H
