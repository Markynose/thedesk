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
#include "icontextchunk.h"
#include "ui_icontextchunk.h"

#include "barmanager.h"
#include "statemanager.h"
#include <QIcon>
#include <cmath>
#include <libcontemporary_global.h>
#include <m3motion.h>

struct IconTextChunkPrivate {
        QString name;
        QWidget* quickWidget = nullptr;

        QString text;
        QString shortText, lastShortText; // the last one stays while it springs away
        qreal percentage = 0;
        M3SpringAnimation* shortShown;
};

IconTextChunk::IconTextChunk(QString name) :
    Chunk(),
    ui(new Ui::IconTextChunk) {
    ui->setupUi(this);
    d = new IconTextChunkPrivate();
    d->name = name;
    d->shortShown = new M3SpringAnimation(this);
    d->shortShown->setToken(M3Motion::SpatialDefault);
    d->shortShown->snapTo(0.0);
    connect(d->shortShown, &M3SpringAnimation::valueChanged, this, &IconTextChunk::updateLabel);

    connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [=](qreal percentage) {
        int padding = 3 + 6 * percentage;
        this->layout()->setContentsMargins(6, padding, 6, padding);
        d->percentage = percentage;
        updateLabel();
    });

    connect(this, &IconTextChunk::clicked, this, [=] {
        showQuickWidget();
    });
}

IconTextChunk::~IconTextChunk() {
    delete d;
    delete ui;
}

void IconTextChunk::setIcon(QIcon icon) {
    ui->iconLabel->setPixmap(icon.pixmap(SC_DPI_T(QSize(16, 16), QSize)));
}

void IconTextChunk::setText(QString text) {
    d->text = text;
    updateLabel();
}

void IconTextChunk::setShortText(QString text) {
    d->shortText = text;
    if (!text.isEmpty()) d->lastShortText = text;
    d->shortShown->animateTo(text.isEmpty() ? 0.0 : 1.0);
    updateLabel();
}

void IconTextChunk::updateLabel() {
    // The label grows from the short text (or nothing) to the full one as the
    // bar opens; the text swaps half way, and the label rolls the change
    const double shown = qBound(0.0, d->shortShown->currentValue().toDouble(), 1.0);
    const bool full = d->percentage >= 0.5 || d->lastShortText.isEmpty();
    ui->textLabel->setText(full ? d->text : d->lastShortText);
    if (shown <= 0 && d->shortText.isEmpty()) d->lastShortText.clear();

    if (qFuzzyCompare(d->percentage, 1)) {
        ui->textLabel->setFixedWidth(QWIDGETSIZE_MAX);
        return;
    }
    const QFontMetricsF metrics(ui->textLabel->font());
    const double shortWidth = d->lastShortText.isEmpty() ? 0 : metrics.horizontalAdvance(d->lastShortText) * shown;
    const double fullWidth = metrics.horizontalAdvance(d->text);
    ui->textLabel->setFixedWidth(int(std::ceil(shortWidth + (fullWidth - shortWidth) * d->percentage)));
}

void IconTextChunk::setQuickWidget(QWidget* widget) {
    d->quickWidget = widget;
}

void IconTextChunk::deleteLater() {
    Chunk::deleteLater();
}

QString IconTextChunk::name() {
    return d->name;
}

int IconTextChunk::expandedHeight() {
    return qMax(SC_DPI(16), this->fontMetrics().height()) + 18;
}

int IconTextChunk::statusBarHeight() {
    return qMax(SC_DPI(16), this->fontMetrics().height()) + 6;
}

QWidget* IconTextChunk::quickWidget() {
    return d->quickWidget;
}
