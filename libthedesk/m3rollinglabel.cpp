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
#include <QStyle>
#include <m3motion.h>

M3RollingLabel::M3RollingLabel(QWidget* parent) :
    QLabel(parent) {
    roll = new M3SpringAnimation(this);
    roll->setToken(M3Motion::SpatialFast);
    roll->snapTo(1.0);
    connect(roll, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

void M3RollingLabel::paintEvent(QPaintEvent* event) {
    // QLabel::setText isn't virtual: notice the change when it repaints
    const QString now = this->text();
    if (now != shown) {
        if (!shown.isEmpty() && !now.isEmpty()) {
            from = shown;
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
    const double travel = metrics.height() * 0.7;
    const double tailX = x + metrics.horizontalAdvance(now.left(same));
    painter.setClipRect(QRectF(tailX - 1, r.top(), r.right() - tailX + 2, r.height()));
    QColor old = colour;
    old.setAlphaF(colour.alphaF() * qBound(0.0, 1 - k, 1.0));
    painter.setPen(old);
    painter.drawText(QPointF(tailX, baseline - k * travel), from.mid(same));
    QColor fresh = colour;
    fresh.setAlphaF(colour.alphaF() * qBound(0.0, k, 1.0));
    painter.setPen(fresh);
    painter.drawText(QPointF(tailX, baseline + (1 - k) * travel), now.mid(same));
}
