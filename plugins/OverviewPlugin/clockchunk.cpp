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
#include "clockchunk.h"
#include "ui_clockchunk.h"

#include <statemanager.h>
#include <barmanager.h>
#include <statuscentermanager.h>
#include <TimeDate/desktoptimedate.h>

namespace {
    // The seconds tick every second: with proportional digits the clock
    // changed width each time and nudged the chunks beside it. Tabular
    // digits are all one width, and the label keeps the width of the
    // widest time so nothing beside it moves
    void steadyDigits(QLabel* label, QFont font) {
        font.setFeature(QFont::Tag("tnum"), 1);
        label->setFont(font);
        label->setMinimumWidth(QFontMetrics(font).horizontalAdvance(QStringLiteral("00:00:00")));
    }
} // namespace

ClockChunk::ClockChunk() :
    Chunk(),
    ui(new Ui::ClockChunk) {
    ui->setupUi(this);
    steadyDigits(ui->time, ui->time->font());

    DesktopTimeDate::makeTimeLabel(ui->time, DesktopTimeDate::Time);
    DesktopTimeDate::makeTimeLabel(ui->ampmLabel, DesktopTimeDate::AmPm);
    DesktopTimeDate::makeTimeLabel(ui->date, DesktopTimeDate::StandardDate);

    connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [ = ](qreal percentage) {
        qreal pointSize = (15 - this->font().pointSizeF()) * percentage + this->font().pointSizeF();
        QFont font = this->font();
        font.setPointSizeF(pointSize);
        // Expressive: the clock swells in weight and width as the bar opens
        font.setVariableAxis(QFont::Tag("wght"), float(650 + 350 * percentage));
        font.setVariableAxis(QFont::Tag("wdth"), float(100 + 25 * percentage));

        steadyDigits(ui->time, font);
        ui->ampmLabel->setFont(font);

        int padding = 3 + 6 * percentage;
        this->layout()->setContentsMargins(0, padding, 0, padding);

        if (qFuzzyCompare(percentage, 1)) {
            ui->date->setFixedWidth(QWIDGETSIZE_MAX);
        } else {
            ui->date->setFixedWidth(ui->date->sizeHint().width() * percentage);
        }
    });

    connect(this, &ClockChunk::clicked, this, [ = ] {
        StateManager::instance()->statusCenterManager()->show();
    });
}

ClockChunk::~ClockChunk() {
    delete ui;
}


QString ClockChunk::name() {
    return "OverviewClock";
}

int ClockChunk::expandedHeight() {
    QFontMetrics metrics(QFont(this->font().family(), 15));
    return metrics.height() + 18;
}

int ClockChunk::statusBarHeight() {
    return this->fontMetrics().height() + 6;
}
