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
#include "tstylemanager.h"

#include <QDir>
#include <tapplication.h>

#ifdef Q_OS_WIN
#include <winrt/base.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.ui.viewmanagement.h>
namespace ViewManagement = winrt::Windows::UI::ViewManagement;
#endif

struct tStyleManagerPrivate {
        tStyleManager::Style currentStyle = tStyleManager::System;
        tApplication::Platforms overrideOnPlatforms = tApplication::Flatpak | tApplication::Windows | tApplication::WindowsAppPackage | tApplication::MacOS | tApplication::OtherPlatform;
#ifdef Q_OS_WIN
        ViewManagement::UISettings settings;
#endif
};

void tStyleManager::updateStyle () {
    if (isOverridingStyle()) {
        // Override the styles!

        QApplication::setStyle(QStyleFactory::create("contemporary"));

        QIcon::setThemeSearchPaths({
            QDir::home().absoluteFilePath(".local/share/icons"),
            "/usr/share/icons",                                           // Linux
            "/app/share/icons",                                           // Flatpak
            tApplication::applicationDirPath() + "\\icons",               // Windows
            tApplication::macOSBundlePath() + "/Contents/Resources/icons" // macOS
        });

        if (qEnvironmentVariableIsSet("THELIBS_OVERRIDE_ICON_SEARCH_PATH")) {
            QIcon::setThemeSearchPaths({ qEnvironmentVariable("THELIBS_OVERRIDE_ICON_SEARCH_PATH") });
        }

        QIcon::setThemeName("contemporary");
        QIcon::setFallbackThemeName("contemporary-icons");

        QPalette pal = QApplication::palette();

        // Get the accent colour
        QColor accentCol;
#ifdef Q_OS_WIN
        try {
            if (d->currentStyle == ContemporaryLight) {
                auto accent = d->settings.GetColorValue(ViewManagement::UIColorType::AccentLight1);             
                accentCol = QColor::fromRgb(accent.R, accent.G, accent.B);
            } else {
                auto accent = d->settings.GetColorValue(ViewManagement::UIColorType::Accent);
                accentCol = QColor::fromRgb(accent.R, accent.G, accent.B);
            }
        } catch (...) {
#endif
            accentCol = QColor::fromRgb((d->currentStyle == ContemporaryLight ? 0xC400C8FF : 0xC4003296) & 0x00FFFFFF);
#ifdef Q_OS_WIN
        }
#endif

        pal.setColor(QPalette::Button, accentCol);

        if (d->currentStyle == ContemporaryLight) {
            pal.setColor(QPalette::ButtonText, QColor(0, 0, 0));
            pal.setColor(QPalette::Highlight, accentCol.lighter(125));
            pal.setColor(QPalette::HighlightedText, QColor(0, 0, 0));
            pal.setColor(QPalette::Disabled, QPalette::Button, accentCol.darker(200));
            pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0, 0, 0));

            pal.setColor(QPalette::Window, QColor(210, 210, 210));
            pal.setColor(QPalette::Base, QColor(210, 210, 210));
            pal.setColor(QPalette::AlternateBase, QColor(210, 210, 210));
            pal.setColor(QPalette::WindowText, QColor(0, 0, 0));
            pal.setColor(QPalette::Text, QColor(0, 0, 0));
            pal.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
            pal.setColor(QPalette::PlaceholderText, QColor(100, 100, 100));

            pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 100, 100));
        } else {
            pal.setColor(QPalette::ButtonText, QColor(255, 255, 255));
            pal.setColor(QPalette::Highlight, accentCol.lighter(125));
            pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
            pal.setColor(QPalette::Disabled, QPalette::Button, accentCol.darker(200));
            pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(150, 150, 150));

            pal.setColor(QPalette::Window, QColor(40, 40, 40));
            pal.setColor(QPalette::Base, QColor(40, 40, 40));
            pal.setColor(QPalette::AlternateBase, QColor(60, 60, 60));
            pal.setColor(QPalette::WindowText, QColor(255, 255, 255));
            pal.setColor(QPalette::Text, QColor(255, 255, 255));
            pal.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
            pal.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));

            pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(150, 150, 150));
        }

        QApplication::setPalette(pal);
        QApplication::setPalette(pal, "QDockWidget");
        QApplication::setPalette(pal, "QToolBar");
    }
}

tStyleManager::tStyleManager(QObject *parent) : QObject(parent) {
    d = new tStyleManagerPrivate();

#ifdef Q_OS_WIN
    d->settings.ColorValuesChanged([](auto &&, auto &&) {
        QMetaObject::invokeMethod(instance(), &tStyleManager::updateStyle, Qt::QueuedConnection);
    });
#endif
}

tStyleManager* tStyleManager::instance() {
    static tStyleManager* instance = new tStyleManager();
    return instance;
}

void tStyleManager::setOverrideStyleForApplication(tStyleManager::Style style) {
    instance()->d->currentStyle = style;
    instance()->updateStyle();
}

void tStyleManager::setOverrideStyleOnPlatforms(tApplication::Platforms platforms) {
    instance()->d->overrideOnPlatforms = platforms;
    instance()->updateStyle();
}

bool tStyleManager::isOverridingStyle() {
    return instance()->d->overrideOnPlatforms & tApplication::currentPlatform();
}
