/****************************************
 *
 *   theDesk - a label whose changed characters roll
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3rollinglabel.h"

#include <QPainter>
#include <cmath>
#include <QStyle>
#include <m3motion.h>

M3RollingLabel::M3RollingLabel(QWidget* parent) :
    QLabel(parent) {
    roll = new M3SpringAnimation(this);
    roll->setToken(M3Motion::SpatialFast);
    roll->snapTo(1.0);
    // The seconds roll every second for as long as the bar is up: only the rolling tail repaints,
    // and the spring stops once what's left of its travel is under a twentieth of a pixel
    connect(roll, &M3SpringAnimation::valueChanged, this, [this](const QVariant& value) {
        const double k = value.toDouble();
        if (roll->state() == QAbstractAnimation::Running && std::abs(1 - k) * travel < 0.05 && std::abs(k - lastK) * travel < 0.05) {
            roll->snapTo(1.0);
            return;
        }
        lastK = k;
        if (tail.isEmpty()) this->update();
        else this->update(tail);
    });
    connect(roll, &QAbstractAnimation::finished, this, qOverload<>(&QWidget::update)); // the settled label, as QLabel draws it
}

void M3RollingLabel::paintEvent(QPaintEvent* event) {
    // QLabel::setText isn't virtual: notice the change when it repaints
    const QString now = this->text();
    if (now != shown) {
        if (!shown.isEmpty() && !now.isEmpty()) {
            from = shown;
            tail = QRect();
            lastK = 0;
            travel = QFontMetricsF(this->font()).height() * 0.7;
            roll->snapTo(0.0);
            roll->animateTo(1.0);
        }
        shown = now;
    }
    if (roll->state() != QAbstractAnimation::Running || from.isEmpty()) {
        QLabel::paintEvent(event);
        return;
    }

    int same = 0;
    while (same < from.size() && same < now.size() && from.at(same) == now.at(same)) same++;

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(this->font());
    const QFontMetricsF metrics(this->font());
    const QRectF r = this->contentsRect();
    const Qt::Alignment align = QStyle::visualAlignment(this->layoutDirection(), this->alignment());
    const double width = metrics.horizontalAdvance(now);
    const double x = align & Qt::AlignRight ? r.right() + 1 - width : align & Qt::AlignHCenter ? r.center().x() - width / 2 : r.left();
    const double baseline = r.top() + (r.height() + metrics.ascent() - metrics.descent()) / 2;
    const QColor colour = this->palette().color(this->foregroundRole());

    painter.setPen(colour);
    painter.drawText(QPointF(x, baseline), now.left(same));

    // The tail: the old one leaves upwards as the new one comes up from below
    const double k = roll->currentValue().toDouble();
    const double tailX = x + metrics.horizontalAdvance(now.left(same));
    painter.setClipRect(QRectF(tailX - 1, r.top(), r.right() - tailX + 2, r.height()));
    tail = QRectF(tailX - 1, r.top(), r.right() - tailX + 2, r.height()).toAlignedRect();
    QColor old = colour;
    old.setAlphaF(colour.alphaF() * qBound(0.0, 1 - k, 1.0));
    painter.setPen(old);
    painter.drawText(QPointF(tailX, baseline - k * travel), from.mid(same));
    QColor fresh = colour;
    fresh.setAlphaF(colour.alphaF() * qBound(0.0, k, 1.0));
    painter.setPen(fresh);
    painter.drawText(QPointF(tailX, baseline + (1 - k) * travel), now.mid(same));
}
