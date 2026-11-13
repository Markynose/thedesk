/****************************************
 *
 *   theDesk - End Session tile (Material 3 Expressive)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "endsessionbutton.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    QFont flex(const QFont& base, double px, double wght, double wdth = 100) {
        QFont font = base;
        font.setFamily(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        return font;
    }

    QColor withAlpha(QColor colour, double a) {
        colour.setAlphaF(a);
        return colour;
    }
} // namespace

struct EndSessionButtonPrivate {
        EndSessionButton::Role role = EndSessionButton::Role::Tile;
        M3SpringAnimation* hover;
        M3SpringAnimation* press;
};

EndSessionButton::EndSessionButton(QWidget* parent) :
    QCommandLinkButton(parent) {
    d = new EndSessionButtonPrivate();
    d->hover = new M3SpringAnimation(this);
    d->hover->setToken(M3Motion::EffectsFast);
    d->hover->snapTo(0.0);
    d->press = new M3SpringAnimation(this);
    d->press->setToken(M3Motion::SpatialFast);
    d->press->snapTo(0.0);
    for (auto* spring : {d->hover, d->press}) connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(this, &QAbstractButton::pressed, this, [this] {
        d->press->animateTo(1.0);
    });
    connect(this, &QAbstractButton::released, this, [this] {
        d->press->animateTo(0.0);
    });
    this->setAttribute(Qt::WA_Hover);
    this->setProperty("m3HoverMerge", false); // it paints its own states
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

EndSessionButton::~EndSessionButton() {
    delete d;
}

void EndSessionButton::setRole(Role role) {
    d->role = role;
    this->setFixedHeight(role == Role::Tile ? 76 : 104);
    this->updateGeometry();
    this->update();
}

QSize EndSessionButton::sizeHint() const {
    return QSize(d->role == Role::Tile ? 150 : 220, d->role == Role::Tile ? 76 : 104);
}

QSize EndSessionButton::minimumSizeHint() const {
    return QSize(110, this->sizeHint().height());
}

bool EndSessionButton::event(QEvent* event) {
    switch (event->type()) {
        case QEvent::HoverEnter:
            d->hover->animateTo(1.0);
            break;
        case QEvent::HoverLeave:
            d->hover->animateTo(0.0);
            d->press->animateTo(0.0);
            break;
        case QEvent::ToolTip:
            if (this->toolTip().isEmpty()) this->setToolTip(this->description());
            break;
        default:
            break;
    }
    return QCommandLinkButton::event(event);
}

void EndSessionButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const M3::Scheme s = M3::currentScheme();
    const bool hero = d->role != Role::Tile;

    QColor container, content;
    switch (d->role) {
        case Role::Primary:
            container = s.primary;
            content = s.onPrimary;
            break;
        case Role::Secondary:
            container = s.secondaryContainer;
            content = s.onSecondaryContainer;
            break;
        case Role::Tile:
            container = s.surfaceContainerHighest;
            content = s.onSurface;
            break;
    }
    if (!this->isEnabled()) {
        container = withAlpha(s.onSurface, 0.12);
        content = withAlpha(s.onSurface, 0.38);
    }

    // Corners: ExtraLarge, squishing to Medium while pressed
    const QRectF r = QRectF(this->rect()).adjusted(2, 2, -2, -2);
    const double press = qBound(-0.2, d->press->currentValue().toDouble(), 1.2);
    const double corner = qBound(8.0, 28 + (12 - 28) * press, r.height() / 2);
    QPainterPath shape;
    shape.addRoundedRect(r, corner, corner);
    p.fillPath(shape, container);
    p.fillPath(shape, withAlpha(content, 0.08 * d->hover->currentValue().toDouble() + 0.10 * qBound(0.0, press, 1.0)));
    if (this->hasFocus() && this->testAttribute(Qt::WA_KeyboardFocusChange)) {
        p.setPen(QPen(s.secondary, 2));
        p.drawPath(shape);
        p.setPen(Qt::NoPen);
    }

    // The symbol: bare on hero tiles, in a tonal circle on the others
    const int iconSize = hero ? 28 : 22;
    QRectF iconRect;
    if (hero) {
        iconRect = QRectF(r.left() + 22, r.top() + 20, iconSize, iconSize);
    } else {
        QRectF circle(r.left() + 14, r.center().y() - 22, 44, 44);
        QPainterPath dot;
        dot.addEllipse(circle);
        p.fillPath(dot, this->isEnabled() ? s.secondaryContainer : withAlpha(s.onSurface, 0.12));
        iconRect = QRectF(circle.center().x() - iconSize / 2.0, circle.center().y() - iconSize / 2.0, iconSize, iconSize);
    }
    QPixmap symbol = this->icon().pixmap(QSize(iconSize, iconSize), this->devicePixelRatioF());
    {
        QPainter tint(&symbol);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(symbol.rect(), hero ? content : (this->isEnabled() ? s.onSecondaryContainer : content));
    }
    p.drawPixmap(iconRect.topLeft(), symbol);

    // Label: Roboto Flex that thickens and widens as you hover, and
    // squeezes narrower as you press
    const double hover = qBound(0.0, d->hover->currentValue().toDouble(), 1.2);
    const double squeeze = qBound(0.0, press, 1.0);
    p.setPen(content);
    if (hero) {
        p.setFont(flex(this->font(), 22, 700 + 250 * hover, 108 + 22 * hover - 18 * squeeze));
        QRectF text(r.left() + 22, r.bottom() - 46, r.width() - 44, 32);
        p.drawText(text, Qt::AlignLeading | Qt::AlignVCenter, p.fontMetrics().elidedText(this->text(), Qt::ElideRight, int(text.width())));
    } else {
        p.setFont(flex(this->font(), 15, 550 + 250 * hover, 100 + 14 * hover - 12 * squeeze));
        QRectF text(r.left() + 68, r.top(), r.width() - 68 - 12, r.height());
        p.drawText(text, Qt::AlignLeading | Qt::AlignVCenter | Qt::TextWordWrap, this->text());
    }
}
