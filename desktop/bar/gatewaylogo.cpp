/****************************************
 *
 *   theDesk - Material 3 Expressive Gateway button
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "gatewaylogo.h"

#include <QAbstractButton>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <gatewaymanager.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>

namespace {
    constexpr int flipAfterMs = 2500; // hover this long and the d turns into a p
    constexpr int flipHoldMs = 1400;
} // namespace

struct GatewayLogoPrivate {
        QWidget* host;
        GatewayLogo::Placement placement;
        int size;
        m3shapes::Morph morph{MaterialShapes::sunny(), MaterialShapes::circle()};
        M3SpringAnimation* spin;  // 0 -> 1: an eighth of a turn
        M3SpringAnimation* round; // 0 -> 1: Sunny -> circle
        M3SpringAnimation* weight; // 0 -> 1: thin -> heavy; dips on hover
        M3SpringAnimation* flip;   // 0 -> 1: d -> p (half a turn)
        QTimer flipTimer;
        QTimer unflipTimer;
};

GatewayLogo::GatewayLogo(QWidget* host, Placement placement, int size) :
    QWidget(host) {
    d = new GatewayLogoPrivate();
    d->host = host;
    d->placement = placement;
    d->size = size;
    // On a button it's the button's hover that counts; in the search bar only
    // the mark's own
    if (placement == Placement::ButtonIcon) {
        this->setAttribute(Qt::WA_TransparentForMouseEvents);
    } else {
        this->setAttribute(Qt::WA_Hover);
        this->setCursor(Qt::ArrowCursor); // not the field's I-beam
    }

    d->spin = new M3SpringAnimation(this);
    d->spin->setToken(M3Motion::SpatialDefault);
    d->spin->snapTo(0.0);
    d->round = new M3SpringAnimation(this);
    d->round->setToken(M3Motion::SpatialFast);
    d->round->snapTo(0.0);
    d->weight = new M3SpringAnimation(this);
    d->weight->setToken(M3Motion::SpatialDefault);
    d->weight->snapTo(1.0);
    d->flip = new M3SpringAnimation(this);
    d->flip->setToken(M3Motion::SpatialSlow);
    d->flip->snapTo(0.0);
    for (auto* spring : {d->spin, d->round, d->weight, d->flip}) connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));

    // Linger on it and the d turns upside down into a p, like the turning
    // letters of the loading screen, then rights itself
    d->flipTimer.setSingleShot(true);
    d->flipTimer.setInterval(flipAfterMs);
    connect(&d->flipTimer, &QTimer::timeout, this, [this] {
        d->flip->animateTo(1.0);
        d->unflipTimer.start();
    });
    d->unflipTimer.setSingleShot(true);
    d->unflipTimer.setInterval(flipHoldMs);
    connect(&d->unflipTimer, &QTimer::timeout, this, [this] {
        d->flip->animateTo(0.0);
    });

    // Round and squeezed while the Gateway is open
    connect(StateManager::gatewayManager(), &GatewayManager::openChanged, this, [this](bool open) {
        d->round->animateTo(open ? 1.0 : 0.0);
    });

    host->installEventFilter(this);
    if (auto* button = qobject_cast<QAbstractButton*>(host)) button->setIcon(QIcon());
    place();
}

GatewayLogo::~GatewayLogo() {
    delete d;
}

void GatewayLogo::place() {
    QRect r(0, 0, d->size, d->size);
    r.moveCenter(d->host->rect().center());
    if (d->placement == Placement::Leading) r.moveLeft(d->host->layoutDirection() == Qt::RightToLeft ? d->host->width() - 14 - d->size : 14);
    this->setGeometry(r);
}

void GatewayLogo::hoverChanged(bool hovered) {
    if (hovered) {
        // Each visit turns it on by another eighth (Sunny has eight rays),
        // and the d dips thin and springs back to its heaviest
        d->spin->animateTo(std::round(d->spin->endValue().toDouble()) + 1.0);
        d->weight->snapTo(0.0);
        d->weight->animateTo(1.0);
        d->flipTimer.start();
    } else {
        d->flipTimer.stop();
        d->unflipTimer.stop();
        d->flip->animateTo(0.0);
    }
}

bool GatewayLogo::event(QEvent* event) {
    if (d->placement == Placement::Leading) {
        if (event->type() == QEvent::HoverEnter) hoverChanged(true);
        if (event->type() == QEvent::HoverLeave) hoverChanged(false);
    }
    return QWidget::event(event);
}

bool GatewayLogo::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
        case QEvent::Resize:
            place();
            break;
        case QEvent::Enter:
            if (d->placement == Placement::ButtonIcon) hoverChanged(true);
            break;
        case QEvent::Leave:
            if (d->placement == Placement::ButtonIcon) hoverChanged(false);
            break;
        case QEvent::PaletteChange:
            this->update();
            break;
        default:
            break;
    }
    return false;
}

void GatewayLogo::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    M3::Scheme scheme = M3::currentScheme();
    const QRectF r = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    painter.save();
    painter.translate(r.center());
    painter.rotate(d->spin->currentValue().toDouble() * 45);
    painter.translate(-r.center());
    painter.fillPath(m3shapes::toPath(d->morph, float(d->round->currentValue().toDouble()), r), scheme.tertiaryContainer);
    painter.restore();

    // The "d": Roboto Flex at its heaviest and widest. Opening the Gateway
    // squeezes it narrow and tall as the shape rounds off
    const double round = qBound(0.0, d->round->currentValue().toDouble(), 1.0);
    const double weight = d->weight->currentValue().toDouble();
    QFont font(QStringLiteral("Roboto Flex"));
    font.setPixelSize(qRound(r.height() * (0.62 + 0.1 * round)));
    font.setVariableAxis(QFont::Tag("opsz"), float(font.pixelSize()));
    font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, 250 + 750 * weight, 1000.0)));
    font.setVariableAxis(QFont::Tag("wdth"), float(151 - 126 * round));
    QPainterPath glyph;
    glyph.setFillRule(Qt::WindingFill);
    glyph.addText(0, 0, font, QStringLiteral("d"));
    painter.translate(r.center());
    painter.rotate(d->flip->currentValue().toDouble() * 180); // d, turned half way round, is a p
    painter.fillPath(glyph.translated(-glyph.boundingRect().center()), scheme.onTertiaryContainer);
}
