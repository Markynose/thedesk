/****************************************
 *
 *   theDesk - loading screen settings
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "splashsettings.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <horizon/horizonview.h>
#include <farewell.h>
#include <livingtypeview.h>
#include <tsettings.h>
#include <tswitch.h>

struct SplashSettingsPrivate {
        tSettings settings{QStringLiteral("theSuite"), QStringLiteral("theDesk")};
        QWidget* card;
        HorizonView* horizon;
        LivingTypeView* view;
        QButtonGroup* exits;
        QButtonGroup* colours;
        tSwitch* seasonal;
        QTimer* replay;
        QTimer* goodbyeReset; // back to the loading phrases after the goodbye
        bool loading = false;
};

namespace {
    QLabel* caption(const QString& text, QWidget* parent) {
        auto* label = new QLabel(text, parent);
        label->setProperty("m3NoHeader", true);
        return label;
    }

    // A connected M3E button group of checkable buttons, one checked at a time
    QWidget* connectedGroup(QWidget* parent, QButtonGroup* group, const QList<QPair<QString, QString>>& choices) {
        auto* row = new QWidget(parent);
        row->setProperty("m3ButtonGroup", QStringLiteral("connected"));
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        for (int i = 0; i < choices.size(); i++) {
            auto* button = new QPushButton(choices.at(i).second, row);
            button->setCheckable(true);
            button->setProperty("m3ButtonSize", QStringLiteral("xs"));
            button->setProperty("value", choices.at(i).first);
            group->addButton(button, i);
            layout->addWidget(button);
        }
        layout->addStretch();
        return row;
    }
} // namespace

SplashSettings::SplashSettings(QWidget* parent) :
    QWidget(parent) {
    d = new SplashSettingsPrivate();
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(12);

    // The preview: the real loading screen in a card, over a small living
    // horizon that its exit opens onto
    d->card = new QWidget(this);
    d->card->setFixedSize(384, 216);
    d->horizon = new HorizonView(d->card);
    d->horizon->setGeometry(d->card->rect());
    d->view = new LivingTypeView(d->card);
    d->view->setPreview(true);
    d->view->setMode(LivingTypeView::Field);
    d->view->setGeometry(d->card->rect());
    QPainterPath rounded;
    rounded.addRoundedRect(QRectF(d->card->rect()), 16, 16);
    d->card->setMask(QRegion(rounded.toFillPolygon().toPolygon()));
    connect(d->view, &LivingTypeView::exitFinished, this, [this] {
        d->horizon->playIntro();
        d->replay->start();
    });
    d->replay = new QTimer(this);
    d->replay->setSingleShot(true);
    d->replay->setInterval(2200);
    connect(d->replay, &QTimer::timeout, this, [this] {
        d->view->show();
        d->view->restart();
    });

    auto* play = new QPushButton(QIcon::fromTheme(QStringLiteral("media-playback-start")), tr("Play the exit"), this);
    play->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    connect(play, &QPushButton::clicked, this, &SplashSettings::playPreview);
    // The goodbye (shutting down, restarting, logging out): the same screen
    // with its words, then the loading phrases again
    auto* goodbye = new QPushButton(QIcon::fromTheme(QStringLiteral("system-shutdown")), tr("Play the goodbye"), this);
    goodbye->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    d->goodbyeReset = new QTimer(this);
    d->goodbyeReset->setSingleShot(true);
    d->goodbyeReset->setInterval(5000);
    connect(d->goodbyeReset, &QTimer::timeout, this, [this] {
        d->view->setFarewell({});
    });
    connect(goodbye, &QPushButton::clicked, this, [this] {
        d->replay->stop();
        d->view->show();
        d->view->setFarewell(Farewell::phrasesFor(Farewell::PowerOff));
        d->goodbyeReset->start(); // one at a time: a second click starts it over
    });
    // A standard M3E button group: the pressed one widens, the other gives way
    auto* playGroup = new QWidget(this);
    playGroup->setProperty("m3ButtonGroup", QStringLiteral("standard"));
    auto* playGroupLayout = new QHBoxLayout(playGroup);
    playGroupLayout->setContentsMargins(0, 0, 0, 0);
    playGroupLayout->setSpacing(8);
    play->setParent(playGroup);
    goodbye->setParent(playGroup);
    playGroupLayout->addWidget(play);
    playGroupLayout->addWidget(goodbye);
    auto* playRow = new QHBoxLayout();
    playRow->addWidget(playGroup);
    playRow->addStretch();
    auto* previewColumn = new QVBoxLayout();
    previewColumn->setSpacing(8);
    previewColumn->addWidget(d->card);
    previewColumn->addLayout(playRow);
    layout->addLayout(previewColumn, 0, 0, 1, 2);

    // The exit
    d->exits = new QButtonGroup(this);
    QList<QPair<QString, QString>> exits = {{QStringLiteral("random"), tr("Surprise me")}};
    const QStringList exitLabels = {tr("Vortex"), tr("Bloom"), tr("Ripple"), tr("Drop"), tr("Sweep"), tr("Cascade")};
    for (int style = 0; style < LivingTypeView::ExitStyleCount; style++) exits.append({LivingTypeView::exitStyleName(style), exitLabels.value(style)});
    layout->addWidget(caption(tr("Exit"), this), 1, 0);
    layout->addWidget(connectedGroup(this, d->exits, exits), 1, 1);
    connect(d->exits, &QButtonGroup::idClicked, this, [this](int id) {
        if (d->loading) return;
        const QString value = d->exits->button(id)->property("value").toString();
        d->settings.setValue(QStringLiteral("Splash/exit"), value);
        d->view->setExitStyle(value == QStringLiteral("random") ? LivingTypeView::RandomExit : LivingTypeView::ExitStyle(id - 1));
        playPreview();
    });

    // The colours
    d->colours = new QButtonGroup(this);
    layout->addWidget(caption(tr("Colours"), this), 2, 0);
    layout->addWidget(connectedGroup(this, d->colours, {{QStringLiteral("scheme"), tr("Your colours")}, {QStringLiteral("mlm"), tr("Pride flag")}, {QStringLiteral("rare"), tr("Now and then")}}), 2, 1);
    for (QAbstractButton* b : d->colours->buttons()) {
        if (b->property("value") == QStringLiteral("mlm")) b->setToolTip(tr("The words in the MLM pride flag's stripes"));
        if (b->property("value") == QStringLiteral("rare")) b->setToolTip(tr("The pride flag about one start in fifteen"));
    }
    connect(d->colours, &QButtonGroup::idClicked, this, [this](int id) {
        if (d->loading) return;
        const QString value = d->colours->button(id)->property("value").toString();
        d->settings.setValue(QStringLiteral("Splash/colours"), value);
        d->view->setFlagColours(value == QStringLiteral("mlm"));
    });

    // The seasonal phrases
    d->seasonal = new tSwitch(this);
    auto* seasonalRow = new QHBoxLayout();
    auto* seasonalText = new QLabel(tr("Seasonal messages: Movember in November, mental health in May and on 10 October"), this);
    seasonalText->setWordWrap(true);
    seasonalRow->addWidget(seasonalText, 1);
    seasonalRow->addWidget(d->seasonal);
    layout->addWidget(caption(tr("Messages"), this), 3, 0);
    layout->addLayout(seasonalRow, 3, 1);
    connect(d->seasonal, &tSwitch::toggled, this, [this](bool on) {
        if (d->loading) return;
        d->settings.setValue(QStringLiteral("Splash/seasonal"), on);
        d->view->restart(); // the phrases are picked when the field is built
    });

    layout->setColumnStretch(1, 1);
    connect(&d->settings, &tSettings::settingChanged, this, [this](QString key) {
        if (key.startsWith(QStringLiteral("Splash/"))) load();
    });
    load();
}

SplashSettings::~SplashSettings() {
    delete d;
}

void SplashSettings::load() {
    d->loading = true;
    const QStringList keys = d->settings.keysInGroup(QStringLiteral("Splash"));
    const QString exit = keys.contains(QStringLiteral("exit")) ? d->settings.value(QStringLiteral("Splash/exit")).toString() : QStringLiteral("random");
    const QString colours = keys.contains(QStringLiteral("colours")) ? d->settings.value(QStringLiteral("Splash/colours")).toString() : QStringLiteral("scheme");
    for (QAbstractButton* b : d->exits->buttons()) b->setChecked(b->property("value").toString() == exit);
    for (QAbstractButton* b : d->colours->buttons()) b->setChecked(b->property("value").toString() == colours);
    d->seasonal->setChecked(!keys.contains(QStringLiteral("seasonal")) || d->settings.value(QStringLiteral("Splash/seasonal")).toBool());
    const int exitId = d->exits->checkedId();
    d->view->setExitStyle(exitId <= 0 ? LivingTypeView::RandomExit : LivingTypeView::ExitStyle(exitId - 1));
    d->view->setFlagColours(colours == QStringLiteral("mlm"));
    d->loading = false;
}

void SplashSettings::playPreview() {
    d->replay->stop();
    // Out of the goodbye first, so its reset can't cut the exit short
    if (d->goodbyeReset->isActive()) {
        d->goodbyeReset->stop();
        d->view->setFarewell({});
    }
    d->view->restart();
    // Let the words settle in, then play the exit
    QTimer::singleShot(1600, d->view, [this] {
        d->view->playExit();
    });
}

void SplashSettings::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    d->view->restart();
}

void SplashSettings::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->replay->stop();
    if (d->goodbyeReset->isActive()) {
        d->goodbyeReset->stop();
        d->view->setFarewell({});
    }
}
