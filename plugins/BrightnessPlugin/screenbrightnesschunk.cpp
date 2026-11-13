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
#include "screenbrightnesschunk.h"
#include "ui_screenbrightnesschunk.h"
#include "backlight.h"

#include <QIcon>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include <barmanager.h>
#include <libcontemporary_global.h>
#include <statemanager.h>

ScreenBrightnessChunk::ScreenBrightnessChunk() :
    Chunk(),
    ui(new Ui::ScreenBrightnessChunk) {
    ui->setupUi(this);

    ui->iconLabel->setPixmap(QIcon::fromTheme("video-display").pixmap(SC_DPI_T(QSize(16, 16), QSize)));
    ui->brightnessSlider->setFixedWidth(SC_DPI(200));

    if (Backlight::available()) {
        StateManager::barManager()->addChunk(this);
        SystemScreen* screen = ScreenDaemon::instance()->primayScreen();
        if (screen) {
            connect(screen, &SystemScreen::screenBrightnessChanged, this, [=] {
                ui->brightnessSlider->setValue(static_cast<int>(Backlight::value() * 100));
            });
        }
        // The kernel's backlight has no change signal: catch up when the bar opens
        connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [this, last = 0.0](qreal percentage) mutable {
            // once as it starts to open (this fires every frame, and reading the backlight is a round trip)
            if (last == 0 && percentage > 0 && !ui->brightnessSlider->isSliderDown()) ui->brightnessSlider->setValue(static_cast<int>(Backlight::value() * 100));
            last = percentage;
        });
        ui->brightnessSlider->setValue(static_cast<int>(Backlight::value() * 100));
    }

    connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [=](qreal percentage) {
        if (qFuzzyCompare(percentage, 1)) {
            this->setFixedWidth(QWIDGETSIZE_MAX);
        } else {
            this->setFixedWidth(static_cast<int>(this->sizeHint().width() * percentage));
        }
    });
}

ScreenBrightnessChunk::~ScreenBrightnessChunk() {
    StateManager::barManager()->removeChunk(this);
    delete ui;
}

QString ScreenBrightnessChunk::name() {
    return "Brightness";
}

int ScreenBrightnessChunk::expandedHeight() {
    return this->sizeHint().height();
}

int ScreenBrightnessChunk::statusBarHeight() {
    return 0;
}

void ScreenBrightnessChunk::on_brightnessSlider_sliderMoved(int position) {
    Backlight::set(qMax(1, position) / 100.0);
}
