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
#include "plugin.h"

#include "backlight.h"
#include "screenbrightnesschunk.h"
#include <cmath>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QKeySequence>
#include <assistanttools.h>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include <hudmanager.h>
#include <keygrab.h>
#include <localemanager.h>
#include <statemanager.h>
#include <tapplication.h>
#include <tsettings.h>

struct PluginPrivate {
        KeyGrab* brightnessUp;
        KeyGrab* brightnessDown;

        ScreenBrightnessChunk* chunk;
};

Plugin::Plugin() {
    d = new PluginPrivate();
}

Plugin::~Plugin() {
    delete d;
}

void Plugin::activate() {
    tApplication::addPluginTranslator(CNTP_TARGET_NAME);
    tSettings::registerDefaults(QDir::cleanPath(qApp->applicationDirPath() + "/../plugins/BrightnessPlugin/thedesk-brightness.conf"));
    tSettings::registerDefaults("/usr/share/defaults/thedesk-brightness.conf");

    d->brightnessUp = new KeyGrab(QKeySequence(Qt::Key_MonBrightnessUp), "brightnessUp");
    d->brightnessDown = new KeyGrab(QKeySequence(Qt::Key_MonBrightnessDown), "brightnessDown");
    // 5% a press, from what it is now (it may have changed elsewhere)
    auto step = [this](double change) {
        if (!Backlight::available()) {
            StateManager::instance()->hudManager()->showHud({
                {"icon",  "video-display" },
                {"title", tr("Brightness")},
                {"text",  tr("This screen's brightness can't be changed")}
            });
            return;
        }
        const double newBrightness = qBound(0.01, std::round((Backlight::value() + change) * 20) / 20, 1.0);
        Backlight::set(newBrightness);
        StateManager::instance()->hudManager()->showHud({
            {"icon",  "video-display-brightness"},
            {"title", tr("Brightness")},
            {"value", newBrightness   }
        });
    };
    connect(d->brightnessUp, &KeyGrab::activated, this, [step] {
        step(0.05);
    });
    connect(d->brightnessDown, &KeyGrab::activated, this, [step] {
        step(-0.05);
    });

    d->chunk = new ScreenBrightnessChunk();

    // For the assistant: a slider card the user can keep moving
    AssistantTools::instance()->add({QStringLiteral("set_brightness"),
        QStringLiteral("Set the built-in screen's brightness, 1 to 100 percent. Returns what it was before."),
        AssistantTools::schemaOf({{"percent", "number"}}), false, [](QJsonObject args) {
            AssistantToolResult result;
            if (!Backlight::available()) {
                result.text = QStringLiteral("This screen's brightness can't be changed.");
                return result;
            }
            const double before = Backlight::value();
            const double value = qBound(0.01, args.value("percent").toDouble() / 100, 1.0);
            Backlight::set(value);
            result.text = QStringLiteral("Brightness is now %1% (was %2%).").arg(qRound(value * 100)).arg(qRound(before * 100));
            result.card = QStringLiteral("level");
            result.icon = QStringLiteral("brightness_6");
            result.title = tr("Brightness");
            result.value = value;
            result.setValue = [](double v) {
                Backlight::set(qBound(0.01, v, 1.0));
            };
            result.undo = [before] {
                Backlight::set(before);
            };
            return result;
        }});
}

void Plugin::deactivate() {
    d->brightnessUp->deleteLater();
    d->brightnessDown->deleteLater();

    d->chunk->deleteLater();
    AssistantTools::instance()->remove(QStringLiteral("set_brightness"));

    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
