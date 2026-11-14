/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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
#include "audiochunk.h"

#include <statemanager.h>
#include <quietmodemanager.h>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include "audioquickwidget.h"

struct AudioChunkPrivate {
    AudioQuickWidget* qw = nullptr;
    QPointer<PulseAudioQt::Sink> sink;
    QTimer volumeShown;
};

AudioChunk::AudioChunk() : IconTextChunk("audio") {
    d = new AudioChunkPrivate();

    connect(StateManager::quietModeManager(), &QuietModeManagerTd::quietModeChanged, this, &AudioChunk::updateData);

    // A volume change shows the level on the closed bar for a moment
    d->volumeShown.setSingleShot(true);
    d->volumeShown.setInterval(1500);
    connect(&d->volumeShown, &QTimer::timeout, this, [this] {
        this->setShortText({});
    });
    connect(PulseAudioQt::Context::instance()->server(), &PulseAudioQt::Server::defaultSinkChanged, this, &AudioChunk::trackSink);
    trackSink(PulseAudioQt::Context::instance()->server()->defaultSink());

    d->qw = new AudioQuickWidget();
    this->setQuickWidget(d->qw);
}

AudioChunk::~AudioChunk() {
    d->qw->deleteLater();
    delete d;
}

void AudioChunk::trackSink(PulseAudioQt::Sink* sink) {
    if (d->sink) d->sink->disconnect(this);
    d->sink = sink;
    if (sink) connect(sink, &PulseAudioQt::Sink::volumeChanged, this, [this] {
        updateData();
        this->setShortText(QStringLiteral("%1%").arg(qRound(100.0 * d->sink->volume() / PulseAudioQt::normalVolume())));
        d->volumeShown.start();
    });
    updateData();
}

void AudioChunk::updateData() {
    this->setIcon(QIcon::fromTheme(QuietModeManagerTd::icon(StateManager::quietModeManager()->currentMode())));
    QString text = QuietModeManagerTd::name(StateManager::quietModeManager()->currentMode());
    if (d->sink) text = QStringLiteral("%1 · %2%").arg(text).arg(qRound(100.0 * d->sink->volume() / PulseAudioQt::normalVolume()));
    this->setText(text);
}
