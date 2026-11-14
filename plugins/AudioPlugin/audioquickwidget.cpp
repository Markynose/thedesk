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
#include "audioquickwidget.h"

#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <m3motion.h>
#include <quietmodemanager.h>
#include <statemanager.h>

#include "common.h"
#include "quickwidgetsinkinput.h"
#include "volumecolumn.h"
#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>

struct AudioQuickWidgetPrivate {
        QScreen* primaryScreen = nullptr;
        static QPointer<AudioQuickWidget> instance;

        QHBoxLayout* columns; // the apps, then the master
        VolumeColumn* master;
        QPointer<PulseAudioQt::Sink> sink;
        bool changingVolume = false;

        QHBoxLayout* quietLayout;
        QMap<QuietModeManagerTd::QuietMode, QPushButton*> quietButtons;
        QList<M3SpringAnimation*> quietGrow;
        QuietModeManagerTd::QuietMode beforeMute = QuietModeManagerTd::Sound;

        QPushButton* output;
        QMenu* outputMenu;

        QMap<PulseAudioQt::SinkInput*, QuickWidgetSinkInput*> sinkInputWidgets;
};

QPointer<AudioQuickWidget> AudioQuickWidgetPrivate::instance;

AudioQuickWidget::AudioQuickWidget(QWidget* parent) :
    QWidget(parent) {
    d = new AudioQuickWidgetPrivate();
    d->instance = this;
    QuietModeManagerTd* quiet = StateManager::quietModeManager();

    // Android's volume dialog, opened wide: quiet modes, a slider per app
    // beside the master, and the output it plays on
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    auto* quietGroup = new QWidget(this);
    quietGroup->setProperty("m3ButtonGroup", QStringLiteral("connected"));
    quietGroup->setMinimumWidth(264);
    d->quietLayout = new QHBoxLayout(quietGroup);
    d->quietLayout->setContentsMargins(0, 0, 0, 0);
    for (QuietModeManagerTd::QuietMode mode : quiet->availableQuietModes()) {
        QPushButton* button = new QPushButton(QIcon::fromTheme(QuietModeManagerTd::icon(mode)), QString(), quietGroup);
        button->setToolTip(QStringLiteral("%1: %2").arg(QuietModeManagerTd::name(mode), QuietModeManagerTd::description(mode)));
        button->setAccessibleName(QuietModeManagerTd::name(mode));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        connect(button, &QPushButton::clicked, quiet, [quiet, mode] {
            quiet->setQuietMode(mode);
        });
        d->quietLayout->addWidget(button);
        d->quietButtons.insert(mode, button);

        // The selected one is wider
        auto* grow = new M3SpringAnimation(this);
        grow->setToken(M3Motion::SpatialFast);
        grow->snapTo(0.0);
        const int index = d->quietGrow.count();
        connect(grow, &M3SpringAnimation::valueChanged, this, [this, index](QVariant value) {
            d->quietLayout->setStretch(index, 100 + qRound(60 * value.toDouble()));
        });
        d->quietGrow.append(grow);
    }
    layout->addWidget(quietGroup);
    connect(quiet, &QuietModeManagerTd::quietModeChanged, this, &AudioQuickWidget::updateQuietMode);

    d->columns = new QHBoxLayout();
    d->columns->setSpacing(0);
    d->columns->addStretch();
    d->master = new VolumeColumn(true, this);
    d->master->setName(tr("Volume"));
    d->columns->addWidget(d->master);
    d->columns->addStretch();
    layout->addLayout(d->columns, 1);
    connect(d->master, &QAbstractSlider::sliderPressed, this, [this] {
        d->changingVolume = true;
    });
    connect(d->master, &QAbstractSlider::sliderReleased, this, [this] {
        d->changingVolume = false;
    });
    connect(d->master, &QAbstractSlider::valueChanged, this, [this](int value) {
        if (d->sink) d->sink->setVolume(PulseAudioQt::normalVolume() * value / 100);
    });
    connect(d->master, &VolumeColumn::iconClicked, this, [this, quiet] {
        // Mute is the quiet mode; unmuting goes back to the one before
        if (quiet->currentMode() == QuietModeManagerTd::Mute) {
            quiet->setQuietMode(d->beforeMute == QuietModeManagerTd::Mute ? QuietModeManagerTd::Sound : d->beforeMute);
        } else {
            d->beforeMute = quiet->currentMode();
            quiet->setQuietMode(QuietModeManagerTd::Mute);
        }
    });

    // "Playing on": choosing an output moves every app to it
    d->output = new QPushButton(this);
    d->output->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    d->outputMenu = new QMenu(this);
    d->output->setMenu(d->outputMenu);
    layout->addWidget(d->output);
    connect(d->outputMenu, &QMenu::aboutToShow, this, [this] {
        d->outputMenu->clear();
        auto* group = new QActionGroup(d->outputMenu);
        PulseAudioQt::Sink* current = PulseAudioQt::Context::instance()->server()->defaultSink();
        for (PulseAudioQt::Sink* sink : PulseAudioQt::Context::instance()->sinks()) {
            QAction* action = d->outputMenu->addAction(QIcon::fromTheme(Common::iconForSink(sink)), Common::titleForSink(sink));
            action->setCheckable(true);
            action->setChecked(sink == current);
            group->addAction(action);
            QPointer<PulseAudioQt::Sink> target = sink;
            connect(action, &QAction::triggered, this, [target] {
                if (!target) return;
                PulseAudioQt::Context::instance()->server()->setDefaultSink(target);
                for (PulseAudioQt::SinkInput* sinkInput : PulseAudioQt::Context::instance()->sinkInputs()) sinkInput->setDeviceIndex(target->index());
            });
        }
    });

    connect(PulseAudioQt::Context::instance()->server(), &PulseAudioQt::Server::defaultSinkChanged, this, &AudioQuickWidget::trackSink);
    trackSink(PulseAudioQt::Context::instance()->server()->defaultSink());

    connect(PulseAudioQt::Context::instance(), &PulseAudioQt::Context::sinkInputAdded, this, &AudioQuickWidget::sinkInputAdded);
    connect(PulseAudioQt::Context::instance(), &PulseAudioQt::Context::sinkInputRemoved, this, &AudioQuickWidget::sinkInputRemoved);
    for (PulseAudioQt::SinkInput* sinkInput : PulseAudioQt::Context::instance()->sinkInputs()) sinkInputAdded(sinkInput);

    updateQuietMode();
    for (M3SpringAnimation* grow : d->quietGrow) grow->snapTo(grow->endValue());

    connect(qApp, &QApplication::primaryScreenChanged, this, &AudioQuickWidget::updatePrimaryScreen);
    updatePrimaryScreen();
}

AudioQuickWidget::~AudioQuickWidget() {
    delete d;
}

QSize AudioQuickWidget::sizeHint() const {
    QSize sizeHint = QWidget::sizeHint();
    if (sizeHint.height() > this->maximumHeight()) sizeHint.setHeight(this->maximumHeight());
    return sizeHint;
}

bool AudioQuickWidget::isOpen() {
    return AudioQuickWidgetPrivate::instance && AudioQuickWidgetPrivate::instance->isVisible();
}

void AudioQuickWidget::trackSink(PulseAudioQt::Sink* sink) {
    if (d->sink) d->sink->disconnect(this);
    d->sink = sink;
    if (sink) {
        connect(sink, &PulseAudioQt::Sink::volumeChanged, this, &AudioQuickWidget::updateMaster);
        connect(sink, &PulseAudioQt::Sink::activePortIndexChanged, this, &AudioQuickWidget::updateMaster);
        connect(sink, &PulseAudioQt::Sink::propertiesChanged, this, &AudioQuickWidget::updateMaster);
    }
    updateMaster();
}

void AudioQuickWidget::updateMaster() {
    d->master->setEnabled(!d->sink.isNull());
    d->output->setEnabled(!d->sink.isNull());
    if (!d->sink) {
        d->output->setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-muted")));
        d->output->setText(tr("No Audio Devices"));
        return;
    }

    d->master->setSymbol(Common::symbolForSink(d->sink));
    d->output->setIcon(QIcon::fromTheme(Common::iconForSink(d->sink)));
    d->output->setText(tr("Playing on %1").arg(Common::titleForSink(d->sink)));

    // The number shows a boost past 100%; the slider stops at 100
    const int volume = qRound(100.0 * d->sink->volume() / PulseAudioQt::normalVolume());
    d->master->setShownLevel(volume);
    if (d->changingVolume) return;
    QSignalBlocker blocker(d->master);
    d->master->setValue(qMin(volume, 100));
}

void AudioQuickWidget::updateQuietMode() {
    const QuietModeManagerTd::QuietMode mode = StateManager::quietModeManager()->currentMode();
    QPushButton* selected = d->quietButtons.value(mode);
    for (int i = 0; i < d->quietLayout->count(); i++) {
        QWidget* button = d->quietLayout->itemAt(i)->widget();
        if (button == selected) selected->setChecked(true);
        d->quietGrow.at(i)->animateTo(button == selected ? 1.0 : 0.0);
    }
    d->master->setMuted(mode == QuietModeManagerTd::Mute);
}

void AudioQuickWidget::sinkInputAdded(PulseAudioQt::SinkInput* sinkInput) {
    QuickWidgetSinkInput* sinkInputWidget = new QuickWidgetSinkInput(sinkInput, this);
    sinkInputWidget->installEventFilter(this);
    d->columns->insertWidget(d->columns->indexOf(d->master), sinkInputWidget);
    d->sinkInputWidgets.insert(sinkInput, sinkInputWidget);
}

void AudioQuickWidget::sinkInputRemoved(PulseAudioQt::SinkInput* sinkInput) {
    QuickWidgetSinkInput* sinkInputWidget = d->sinkInputWidgets.take(sinkInput);
    if (!sinkInputWidget) return;
    d->columns->removeWidget(sinkInputWidget);
    sinkInputWidget->setVisible(false);
    sinkInputWidget->deleteLater();
}

bool AudioQuickWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
        this->updateGeometry();

        QEvent* event = new QEvent(QEvent::LayoutRequest);
        qApp->postEvent(this, event);
    }
    return false;
}

void AudioQuickWidget::updatePrimaryScreen() {
    if (d->primaryScreen) d->primaryScreen->disconnect(this);
    d->primaryScreen = qApp->primaryScreen();
    connect(d->primaryScreen, &QScreen::geometryChanged, this, &AudioQuickWidget::updateMaxHeight);
    updateMaxHeight();
}

void AudioQuickWidget::updateMaxHeight() {
    this->setMaximumHeight(d->primaryScreen->geometry().height() * 0.7);
}
