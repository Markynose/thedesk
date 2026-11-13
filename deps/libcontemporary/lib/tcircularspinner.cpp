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

#include "tcircularspinner.h"

#include <QStyle>
#include <QStyleOption>

namespace {
    // theDesk's Material 3 style draws the M3 Expressive loading indicator
    // (morphing shapes) in place of the arc when it answers this style hint
    constexpr auto M3StyleHint = QStyle::StyleHint(QStyle::SH_CustomBase + 0x4D33);
    constexpr auto PE_M3LoadingIndicator = QStyle::PrimitiveElement(QStyle::PE_CustomBase + 0x4D34);
} // namespace

tCircularSpinner::tCircularSpinner(QWidget* parent) :
    QWidget(parent) {
    rotationTimer = new QTimer();
    rotationTimer->setInterval(20);
    connect(rotationTimer, &QTimer::timeout, [this] {
        this->rotation -= 16 * 5;
        this->update();
    });
    connect(this, SIGNAL(destroyed(QObject*)), rotationTimer, SLOT(stop()));
    rotationTimer->start();

    this->arcLength = -16 * 5;

    appear();
}

tCircularSpinner::~tCircularSpinner() {
    rotationTimer->deleteLater();
}

QSize tCircularSpinner::sizeHint() const {
    return QSize(32, 32);
}

void tCircularSpinner::appear() {
    rotation += this->arcLength;
    this->arcLength = -this->arcLength;

    /*//Wait 250ms
    QTimer::singleShot(250, [=] {*/
    // Increase arc length to 325 degrees
    tVariantAnimation* a = new tVariantAnimation();
    a->setStartValue(-16 * 5);
    a->setEndValue(-16 * 300);
    a->setDuration(500);
    a->setEasingCurve(QEasingCurve::InOutCubic);
    connect(a, &tVariantAnimation::valueChanged, [this](QVariant value) {
        arcLength = value.toInt();
        this->update();
    });
    connect(this, SIGNAL(destroyed(QObject*)), a, SLOT(stop()));
    connect(this, SIGNAL(destroyed(QObject*)), a, SLOT(deleteLater()));
    connect(a, SIGNAL(finished()), this, SLOT(disappear()));
    connect(a, SIGNAL(finished()), a, SLOT(deleteLater()));
    a->start();
    //});
}

void tCircularSpinner::disappear() {
    rotation += this->arcLength;
    this->arcLength = -this->arcLength;

    // Wait 250ms
    QTimer* timer = new QTimer();
    timer->setInterval(250);
    connect(timer, &QTimer::timeout, [this, timer] {
        timer->deleteLater();

        // Decrease arc length to 15 degrees
        tVariantAnimation* a = new tVariantAnimation();
        a->setStartValue(this->arcLength);
        a->setEndValue(16 * 5);
        a->setDuration(500);
        a->setEasingCurve(QEasingCurve::InOutCubic);
        connect(a, &tVariantAnimation::valueChanged, [this](QVariant value) {
            arcLength = value.toInt();
            this->update();
        });
        connect(this, SIGNAL(destroyed(QObject*)), a, SLOT(stop()));
        connect(this, SIGNAL(destroyed(QObject*)), a, SLOT(deleteLater()));
        connect(a, SIGNAL(finished()), this, SLOT(appear()));
        connect(a, SIGNAL(finished()), a, SLOT(deleteLater()));
        a->start();
    });
    connect(this, SIGNAL(destroyed(QObject*)), timer, SLOT(stop()));
    connect(this, SIGNAL(destroyed(QObject*)), timer, SLOT(deleteLater()));
    timer->start();
}

void tCircularSpinner::paintEvent(QPaintEvent* event) {
    if (this->style()->styleHint(M3StyleHint, nullptr, this)) {
        QPainter p(this);
        QStyleOption option;
        option.initFrom(this);
        this->style()->drawPrimitive(PE_M3LoadingIndicator, &option, &p, this);
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect spinnerArea;
    spinnerArea.setHeight(qMin(this->width(), this->height()));
    spinnerArea.setWidth(spinnerArea.height());
    spinnerArea.moveLeft(this->width() / 2 - spinnerArea.width() / 2);
    spinnerArea.moveTop(this->height() / 2 - spinnerArea.height() / 2);

    int width = spinnerArea.width() / 12;
    if (width < 2) width = 2;
    int adjFactor = width / 2;
    p.setPen(QPen(this->palette().brush(QPalette::WindowText), width));

    spinnerArea.adjust(adjFactor, adjFactor, -adjFactor, -adjFactor);
    // p.drawArc(spinnerArea, this->rotation, 16 * 45);
    p.drawArc(spinnerArea, this->rotation, this->arcLength);
}
