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
#include "penbutton.h"

#include <QIcon>

#include <QPainter>
#include <QSvgRenderer>
#include <QFile>
#include <tvariantanimation.h>

struct PenButtonPrivate {
    PenButton::PenType penType;
    QColor color;

    tVariantAnimation* heightAnim;
    QSvgRenderer* renderer;
};

PenButton::PenButton(PenType penType, QColor color, QWidget* parent) : QPushButton(parent) {
    d = new PenButtonPrivate();
    d->color = color;
    d->penType = penType;

    QString data;
    if (penType == Pen) {
        this->setFixedSize(40, 48);

        QFile penFile(":/screenshot/icons/pen.svg");
        penFile.open(QFile::ReadOnly);
        data = penFile.readAll();
        data = data.arg(color.name(QColor::HexRgb));
    } else {
        this->setFixedSize(40, 48);

        QFile penFile(":/screenshot/icons/eraser.svg");
        penFile.open(QFile::ReadOnly);
        data = penFile.readAll();
        data = data.arg(tr("Erase-o", "This and the next translation span two lines. Be spiffy with these!"))
            .arg(tr("matic 2000", "This and the previous translation span two lines. Be spiffy with these!"))
            .arg(this->font().family());
    }

    d->heightAnim = new tVariantAnimation(this);
    d->heightAnim->setStartValue(0);
    d->heightAnim->setEndValue(0);
    d->heightAnim->setEasingCurve(QEasingCurve::OutCubic);
    d->heightAnim->setDuration(250);
    connect(d->heightAnim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        this->setFixedHeight(value.toInt());
    });

    d->renderer = new QSvgRenderer();
    d->renderer->load(data.toUtf8());

    this->setCheckable(true);
    this->setAutoExclusive(true);
    this->setAttribute(Qt::WA_Hover);
    this->setToolTip(penType == Pen ? tr("Pen") : tr("Eraser"));
}

PenButton::~PenButton() {
    d->renderer->deleteLater();
    delete d;
}

QColor PenButton::color() const {
    return d->color;
}

void PenButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    // theDesk M3E: a colour swatch in the floating toolbar (the chosen one
    // ringed, as Android's colour pickers do); the eraser is its symbol
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF centre = QRectF(this->rect()).center();
    const QColor ink = this->palette().color(QPalette::WindowText);
    if (this->underMouse()) {
        QColor layer = ink;
        layer.setAlphaF(0.08);
        painter.setPen(Qt::NoPen);
        painter.setBrush(layer);
        painter.drawEllipse(centre, 20, 20);
    }
    if (this->isChecked()) {
        painter.setPen(QPen(ink, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(centre, 17, 17);
    }
    if (d->penType == Pen) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(d->color);
        painter.drawEllipse(centre, 12, 12);
    } else {
        QIcon::fromTheme(QStringLiteral("draw-eraser")).paint(&painter, QRect((centre - QPointF(10, 10)).toPoint(), QSize(20, 20)));
    }
}

void PenButton::updateChecked() {
    this->update();
}


void PenButton::checkStateSet() {
    updateChecked();
}

void PenButton::nextCheckState() {
    this->setChecked(!this->isChecked());
    updateChecked();
}
