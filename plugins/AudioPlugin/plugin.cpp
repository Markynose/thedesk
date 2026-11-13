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

#include "audiochunk.h"
#include "eventhandler.h"
#include "micchunk.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <assistanttools.h>
#include <barmanager.h>
#include <localemanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tapplication.h>
#include <tsettings.h>

struct PluginPrivate {
        EventHandler* keyHandler;
        AudioChunk* chunk;
        MicChunk* micChunk;
};

Plugin::Plugin() {
    d = new PluginPrivate();
}

Plugin::~Plugin() {
    delete d;
}

void Plugin::activate() {
    tApplication::addPluginTranslator(CNTP_TARGET_NAME);

    tSettings::registerDefaults(QDir::cleanPath(qApp->applicationDirPath() + "/../plugins/AudioPlugin/thedesk-audio.conf"));
    tSettings::registerDefaults("/usr/share/defaults/thedesk-audio.conf");

    d->keyHandler = new EventHandler();

    d->chunk = new AudioChunk();
    StateManager::barManager()->addChunk(d->chunk);

    d->micChunk = new MicChunk();

    // For the assistant: the speaker's volume as a slider card, and mute
    AssistantTools::instance()->add({QStringLiteral("set_volume"),
        QStringLiteral("Set the speaker volume, 0 to 100 percent (unmutes). Returns what it was before."),
        AssistantTools::schemaOf({{"percent", "number"}}), false, [](QJsonObject args) {
            AssistantToolResult result;
            PulseAudioQt::Sink* sink = PulseAudioQt::Context::instance()->server()->defaultSink();
            if (!sink) {
                result.text = QStringLiteral("There's no speaker.");
                return result;
            }
            const qint64 normal = PulseAudioQt::normalVolume();
            const qint64 before = sink->volume();
            const bool wasMuted = sink->isMuted();
            const double value = qBound(0.0, args.value("percent").toDouble() / 100, 1.0);
            sink->setVolume(qint64(value * normal));
            sink->setMuted(false);
            result.text = QStringLiteral("Volume is now %1% (was %2%%3).").arg(qRound(value * 100)).arg(qRound(100.0 * before / normal)).arg(wasMuted ? QStringLiteral(", muted") : QString());
            result.card = QStringLiteral("level");
            result.icon = QStringLiteral("volume_up");
            result.title = tr("Volume");
            result.value = value;
            result.setValue = [](double v) {
                if (PulseAudioQt::Sink* s = PulseAudioQt::Context::instance()->server()->defaultSink()) s->setVolume(qint64(qBound(0.0, v, 1.0) * PulseAudioQt::normalVolume()));
            };
            result.undo = [before, wasMuted] {
                if (PulseAudioQt::Sink* s = PulseAudioQt::Context::instance()->server()->defaultSink()) {
                    s->setVolume(before);
                    s->setMuted(wasMuted);
                }
            };
            return result;
        }});
    AssistantTools::instance()->add({QStringLiteral("set_muted"),
        QStringLiteral("Mute or unmute the speaker."),
        AssistantTools::schemaOf({{"muted", "boolean"}}), false, [](QJsonObject args) {
            AssistantToolResult result;
            PulseAudioQt::Sink* sink = PulseAudioQt::Context::instance()->server()->defaultSink();
            if (!sink) {
                result.text = QStringLiteral("There's no speaker.");
                return result;
            }
            const bool before = sink->isMuted();
            const bool muted = args.value("muted").toBool();
            sink->setMuted(muted);
            result.text = muted ? QStringLiteral("Muted.") : QStringLiteral("Unmuted.");
            result.card = QStringLiteral("switch");
            result.icon = muted ? QStringLiteral("volume_off") : QStringLiteral("volume_up");
            result.title = tr("Mute");
            result.value = muted ? 1 : 0;
            result.setValue = [](double on) {
                if (PulseAudioQt::Sink* s = PulseAudioQt::Context::instance()->server()->defaultSink()) s->setMuted(on > 0.5);
            };
            result.undo = [before] {
                if (PulseAudioQt::Sink* s = PulseAudioQt::Context::instance()->server()->defaultSink()) s->setMuted(before);
            };
            return result;
        }});
}

void Plugin::deactivate() {
    d->micChunk->deleteLater();
    StateManager::barManager()->removeChunk(d->chunk);
    d->chunk->deleteLater();
    d->keyHandler->deleteLater();
    AssistantTools::instance()->remove(QStringLiteral("set_volume"));
    AssistantTools::instance()->remove(QStringLiteral("set_muted"));
    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
