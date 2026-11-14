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
#include "quickwidgetsinkinput.h"

#include "common.h"
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <QActionGroup>
#include <QMenu>
#include <QPointer>
#include <libcontemporary_global.h>

struct QuickWidgetSinkInputPrivate {
        bool changingVolume = false;
        PulseAudioQt::SinkInput* sinkInput;

        QString pid;
        static QMultiMap<QString, QuickWidgetSinkInput*> sinkInputsByPid;

        QMenu* devicesMenu;
        QMap<PulseAudioQt::Sink*, QAction*> sinkActions;
        QActionGroup* devicesGroup;
};

QMultiMap<QString, QuickWidgetSinkInput*> QuickWidgetSinkInputPrivate::sinkInputsByPid = QMultiMap<QString, QuickWidgetSinkInput*>();

QuickWidgetSinkInput::QuickWidgetSinkInput(PulseAudioQt::SinkInput* sinkInput, QWidget* parent) :
    VolumeColumn(false, parent) {
    d = new QuickWidgetSinkInputPrivate();
    d->sinkInput = sinkInput;
    connect(this, &QAbstractSlider::sliderPressed, this, [this] {
        d->changingVolume = true;
    });
    connect(this, &QAbstractSlider::sliderReleased, this, [this] {
        d->changingVolume = false;
    });
    connect(this, &QAbstractSlider::valueChanged, this, &QuickWidgetSinkInput::volumeSet);

    connect(sinkInput, &PulseAudioQt::SinkInput::nameChanged, this, &QuickWidgetSinkInput::updateClient);
    connect(sinkInput, &PulseAudioQt::SinkInput::clientChanged, this, &QuickWidgetSinkInput::updateClient);
    connect(sinkInput, &PulseAudioQt::SinkInput::volumeChanged, this, &QuickWidgetSinkInput::updateVolume);
    connect(sinkInput, &PulseAudioQt::SinkInput::propertiesChanged, this, &QuickWidgetSinkInput::updateProperties);
    updateClient();
    updateVolume();
    updateProperties();

    d->devicesGroup = new QActionGroup(this);
    d->devicesGroup->setExclusive(true);

    d->devicesMenu = new QMenu();
    d->devicesMenu->setTitle(tr("Play on"));
    d->devicesMenu->setIcon(QIcon::fromTheme("audio-headphones"));

    this->setMenu(d->devicesMenu);

    connect(PulseAudioQt::Context::instance(), &PulseAudioQt::Context::sinkAdded, this, &QuickWidgetSinkInput::sinkAdded);
    connect(PulseAudioQt::Context::instance(), &PulseAudioQt::Context::sinkRemoved, this, &QuickWidgetSinkInput::sinkRemoved);
    for (PulseAudioQt::Sink* sink : PulseAudioQt::Context::instance()->sinks()) sinkAdded(sink);

    // Not on the output everything else uses: say where it plays
    connect(sinkInput, &PulseAudioQt::SinkInput::deviceIndexChanged, this, &QuickWidgetSinkInput::updateDevice);
    connect(PulseAudioQt::Context::instance()->server(), &PulseAudioQt::Server::defaultSinkChanged, this, &QuickWidgetSinkInput::updateDevice);
    updateDevice();
}

QuickWidgetSinkInput::~QuickWidgetSinkInput() {
    d->sinkInputsByPid.remove(d->pid, this);
    for (QuickWidgetSinkInput* sinkInputWidget : d->sinkInputsByPid.values(d->pid)) sinkInputWidget->updateVisibility();
    delete d->devicesMenu;
    delete d;
}

void QuickWidgetSinkInput::updateVolume() {
    if (d->changingVolume) return;
    int volume = d->sinkInput->volume() / static_cast<double>(PulseAudioQt::normalVolume()) * 100 + 0.5;
    QSignalBlocker blocker(this);
    this->setValue(volume);
}

void QuickWidgetSinkInput::updateClient() {
    QString name = d->sinkInput->properties().value("application.name", d->sinkInput->name()).toString();
    //    if (d->sinkInput->client()) {
    //        name = d->sinkInput->client()->name();
    //    } else {
    //        name = d->sinkInput->name();
    //    }
    this->setName(name);
    // shortcut: a browser or a generic note, until apps tell us more than their name
    const QString app = (name + d->sinkInput->properties().value("application.process.binary").toString()).toLower();
    const bool browser = app.contains("firefox") || app.contains("chrom") || app.contains("browser") || app.contains("brave") || app.contains("edge");
    this->setSymbol(browser ? QStringLiteral("public") : QStringLiteral("music_note"));
}

void QuickWidgetSinkInput::updateProperties() {
    d->sinkInputsByPid.remove(d->pid, this);
    for (QuickWidgetSinkInput* sinkInputWidget : d->sinkInputsByPid.values(d->pid)) sinkInputWidget->updateVisibility();

    d->pid = d->sinkInput->properties().value("application.process.id").toString();
    d->sinkInputsByPid.insert(d->pid, this);
    for (QuickWidgetSinkInput* sinkInputWidget : d->sinkInputsByPid.values(d->pid)) sinkInputWidget->updateVisibility();
}

void QuickWidgetSinkInput::updateVisibility() {
    this->setVisible(d->sinkInputsByPid.values(d->pid).first() == this);
}

void QuickWidgetSinkInput::updateDevice() {
    PulseAudioQt::Sink* defaultSink = PulseAudioQt::Context::instance()->server()->defaultSink();
    QString subtitle;
    if (defaultSink && d->sinkInput->deviceIndex() != defaultSink->index()) {
        for (PulseAudioQt::Sink* sink : PulseAudioQt::Context::instance()->sinks()) {
            if (sink->index() == d->sinkInput->deviceIndex()) subtitle = tr("on %1").arg(Common::titleForSink(sink));
        }
    }
    this->setSubtitle(subtitle);
}

void QuickWidgetSinkInput::volumeSet(int value) {
    qint64 newVolume = PulseAudioQt::normalVolume() * value / 100;

    for (QuickWidgetSinkInput* sinkInputWidget : d->sinkInputsByPid.values(d->pid)) sinkInputWidget->d->sinkInput->setVolume(newVolume);
}

void QuickWidgetSinkInput::sinkAdded(PulseAudioQt::Sink* sink) {
    QAction* action = new QAction(this);
    action->setCheckable(true);

    connect(sink, &PulseAudioQt::Sink::propertiesChanged, action, [=] {
        action->setText(Common::titleForSink(sink));
    });
    action->setText(Common::titleForSink(sink));

    connect(d->sinkInput, &PulseAudioQt::SinkInput::deviceIndexChanged, action, [=] {
        action->setChecked(sink->index() == d->sinkInput->deviceIndex());
    });
    action->setChecked(sink->index() == d->sinkInput->deviceIndex());

    connect(action, &QAction::toggled, this, [=](bool checked) {
        if (checked) {
            for (QuickWidgetSinkInput* sinkInputWidget : d->sinkInputsByPid.values(d->pid)) sinkInputWidget->d->sinkInput->setDeviceIndex(sink->index());
        }
    });

    d->devicesMenu->addAction(action);
    d->devicesGroup->addAction(action);
    d->sinkActions.insert(sink, action);
}

void QuickWidgetSinkInput::sinkRemoved(PulseAudioQt::Sink* sink) {
    QAction* action = d->sinkActions.take(sink);
    d->devicesMenu->removeAction(action);
    d->devicesGroup->removeAction(action);
    action->deleteLater();
}
