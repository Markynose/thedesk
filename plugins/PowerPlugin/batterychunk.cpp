/****************************************
 *
 *   theDesk - the bar's battery
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "batterychunk.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>
#include <barmanager.h>
#include <m3motion.h>
#include <m3rollinglabel.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>

namespace {
    constexpr int Closed = 26, Open = 30; // the chip's height

    QFont labelFont(int px) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("wght"), 600);
        return font;
    }

    QFont symbolFont(int px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), 20);
        return font;
    }

    double clamp01(double t) {
        return qBound(0.0, t, 1.0);
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = clamp01(t);
        return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
            float(a.blueF() + (b.blueF() - a.blueF()) * t), float(a.alphaF() + (b.alphaF() - a.alphaF()) * t));
    }

    // Wavy circular progress, clockwise from the top: the wavy level, then the track after a gap
    void wavyRing(QPainter& p, QPointF c, double r, double level, double phase, const QColor& active, const QColor& track) {
        constexpr double stroke = 2.4, amplitude = 0.9;
        const double filled = 360 * clamp01(level), gap = qRadiansToDegrees((stroke + 3) / r);
        p.setBrush(Qt::NoBrush);
        if (filled + 2 * gap < 360) {
            p.setPen(QPen(track, stroke, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), int((90 - filled - gap) * 16), int(-(360 - filled - 2 * gap) * 16));
        }
        if (filled > 1) {
            QPainterPath path;
            const double waves = qMax(3.0, std::round(2 * M_PI * r / 14));
            const int steps = qMax(8, int(filled / 2));
            for (int i = 0; i <= steps; i++) {
                const double degrees = filled * i / steps, a = qDegreesToRadians(degrees - 90);
                const QPointF point = c + QPointF(std::cos(a), std::sin(a)) * (r + amplitude * std::sin(waves * qDegreesToRadians(degrees) + phase));
                if (i == 0) path.moveTo(point);
                else path.lineTo(point);
            }
            p.setPen(QPen(active, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(path);
        }
        p.setPen(Qt::NoPen);
    }

    M3SpringAnimation* spring(QObject* parent, M3Motion::Token token) {
        auto* animation = new M3SpringAnimation(parent);
        animation->setToken(token);
        animation->snapTo(0.0);
        return animation;
    }
} // namespace

struct BatteryChunk::Private {
        M3RollingLabel* label;
        const QFont closedFont = labelFont(13), openFont = labelFont(14);
        const QFont closedSymbol = symbolFont(11), openSymbol = symbolFont(13);
        M3::Scheme scheme;
        double open = 0; // the bar: 0 closed, 1 open

        int percentage = -1;
        bool onBattery = false, charging = false, fresh = false;
        qint64 seconds = 0;
        QString state, shortText, longText;

        M3SpringAnimation* width; // the label's, so the chip springs to fit
        M3SpringAnimation* low;   // errorContainer
        M3SpringAnimation* plugged; // primaryContainer, a moment after the charger goes in
        M3SpringAnimation* critical; // the SoftBurst out of the ring
        M3SpringAnimation* bolt;
        QTimer flash;

        // While low the wave drifts (and at 5% the burst turns and beats): the ring alone, 20 fps
        QTimer drift;
        QElapsedTimer clock;

        QPainterPath burst; // the morph's path at burstAt, in a unit square
        float burstAt = -1;
};

BatteryChunk::BatteryChunk() :
    Chunk() {
    d = new Private();
    d->scheme = M3::currentScheme();

    d->label = new M3RollingLabel(this);
    d->label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    d->label->setFont(d->closedFont);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(Closed + 2, 0, 12, 0);
    layout->setSpacing(0);
    layout->addWidget(d->label);

    d->width = spring(this, M3Motion::SpatialDefault);
    connect(d->width, &M3SpringAnimation::valueChanged, this, [this](const QVariant& value) {
        d->label->setFixedWidth(qMax(0, qCeil(value.toDouble())));
        this->update();
    });
    d->low = spring(this, M3Motion::EffectsDefault);
    d->plugged = spring(this, M3Motion::EffectsDefault);
    for (M3SpringAnimation* colour : {d->low, d->plugged}) connect(colour, &M3SpringAnimation::valueChanged, this, &BatteryChunk::recolour);
    d->critical = spring(this, M3Motion::SpatialDefault);
    d->bolt = spring(this, M3Motion::EffectsDefault);
    for (M3SpringAnimation* ring : {d->critical, d->bolt}) connect(ring, &M3SpringAnimation::valueChanged, this, [this] {
        this->update(ringRect());
    });
    recolour();

    d->flash.setSingleShot(true);
    d->flash.setInterval(1400);
    connect(&d->flash, &QTimer::timeout, this, [this] {
        d->fresh = false;
        d->plugged->animateTo(0.0);
        retext(true);
    });

    d->drift.setInterval(50);
    connect(&d->drift, &QTimer::timeout, this, [this] {
        this->update(ringRect());
    });

    connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [this](qreal percentage) {
        d->open = percentage;
        this->layout()->setContentsMargins(qRound(Closed + (Open - Closed) * percentage) + 2, 0, 12, 0);
        retext(false);
        this->update();
    });
}

BatteryChunk::~BatteryChunk() {
    delete d;
}

QString BatteryChunk::span(qint64 seconds) {
    if (seconds < 3600) return tr("%1 min").arg(qMax<qint64>(1, seconds / 60));
    return seconds % 3600 < 60 ? tr("%1 h").arg(seconds / 3600) : tr("%1 h %2 min").arg(seconds / 3600).arg(seconds % 3600 / 60);
}

void BatteryChunk::setBattery(int percentage, bool onBattery, bool charging, qint64 seconds, const QString& state) {
    // The charger going in (not the state found at start)
    if (charging && !d->charging && d->percentage >= 0) {
        d->fresh = true;
        d->flash.start();
    }
    if (!charging) {
        d->fresh = false;
        d->flash.stop();
    }
    d->percentage = qBound(0, percentage, 100);
    d->onBattery = onBattery;
    d->charging = charging;
    d->seconds = seconds;
    d->state = state;

    const bool low = onBattery && d->percentage <= 20;
    d->low->animateTo(low ? 1.0 : 0.0);
    d->plugged->animateTo(d->fresh ? 1.0 : 0.0);
    d->critical->animateTo(onBattery && d->percentage <= 5 ? 1.0 : 0.0);
    d->bolt->animateTo(charging ? 1.0 : 0.0);
    retext(true);

    // Calmer motion leaves the wave still
    if (low && M3Motion::expressive()) {
        if (!d->drift.isActive()) {
            d->clock.start();
            d->drift.start();
        }
    } else if (d->drift.isActive()) {
        d->drift.stop();
    }
    this->update();
}

void BatteryChunk::retext(bool animate) {
    const QString figure = QStringLiteral("%1%").arg(d->percentage);
    const bool critical = d->onBattery && d->percentage <= 5, low = d->onBattery && d->percentage <= 20;
    const QString left = d->seconds > 0 ? span(d->seconds) : QString();
    if (critical) {
        d->shortText = tr("%1 · Plug in").arg(figure);
        d->longText = left.isEmpty() ? d->shortText : tr("%1 · Plug in · about %2 left").arg(figure, left);
    } else if (d->onBattery && !left.isEmpty()) {
        d->shortText = low ? tr("%1 · %2").arg(figure, left) : figure;
        d->longText = tr("%1 · about %2 left").arg(figure, left);
    } else if (d->charging) {
        d->shortText = d->fresh ? tr("%1 · Charging").arg(figure) : figure;
        d->longText = left.isEmpty() ? tr("%1 · Charging").arg(figure) : tr("%1 · Charging · full in %2").arg(figure, left);
    } else {
        d->shortText = figure;
        d->longText = d->state.isEmpty() ? figure : tr("%1 · %2").arg(figure, d->state);
    }

    // The words swap half way as the bar opens (the label rolls the change)
    const bool full = d->open >= 0.5;
    d->label->setFont(full ? d->openFont : d->closedFont);
    d->label->setText(full ? d->longText : d->shortText);
    const double shortWidth = QFontMetricsF(d->closedFont).horizontalAdvance(d->shortText);
    const double longWidth = QFontMetricsF(d->openFont).horizontalAdvance(d->longText);
    const double width = shortWidth + (longWidth - shortWidth) * d->open;
    if (animate) d->width->animateTo(width);
    else d->width->snapTo(width);
}

void BatteryChunk::recolour() {
    const M3::Scheme& s = d->scheme;
    const QColor ink = mix(mix(s.onSurface, s.onErrorContainer, d->low->currentValue().toDouble()), s.onPrimaryContainer, d->plugged->currentValue().toDouble());
    QPalette palette = d->label->palette();
    palette.setColor(QPalette::WindowText, ink);
    d->label->setPalette(palette);
    this->update();
}

QRect BatteryChunk::ringRect() const {
    const double h = Closed + (Open - Closed) * d->open;
    return QRectF(0, (this->height() - h) / 2, h + 2, h).toAlignedRect();
}

void BatteryChunk::changeEvent(QEvent* event) {
    // The scheme is fetched here, not per frame
    if (event->type() == QEvent::PaletteChange) {
        d->scheme = M3::currentScheme();
        recolour();
    }
    Chunk::changeEvent(event);
}

void BatteryChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme& s = d->scheme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    const double low = d->low->currentValue().toDouble(), plugged = d->plugged->currentValue().toDouble();
    const QColor box = mix(mix(s.surfaceContainerHighest, s.errorContainer, low), s.primaryContainer, plugged);
    const QColor ink = mix(mix(s.onSurface, s.onErrorContainer, low), s.onPrimaryContainer, plugged);
    const double e = d->open, h = Closed + (Open - Closed) * e, cy = this->height() / 2.0;
    const QRectF pill(0, cy - h / 2, this->width(), h);
    p.setBrush(box);
    p.drawRoundedRect(pill, h / 2, h / 2);

    const double t = d->drift.isActive() ? d->clock.elapsed() / 1000.0 : 0;
    const QPointF lead(pill.left() + h / 2, cy);
    const double r = 7 + 2 * e;
    QColor track = ink;
    track.setAlphaF(0.28f);
    // the wave drifts back toward the start
    wavyRing(p, lead, r, d->percentage / 100.0, -t * 5, mix(mix(s.primary, s.error, low), s.primary, plugged), track);

    // Critical: a turning SoftBurst grows out of the ring, beating, the plug in it
    const double critical = qMax(0.0, d->critical->currentValue().toDouble());
    if (critical > 0.01) {
        const float at = float(clamp01(critical));
        if (at != d->burstAt) {
            static const m3shapes::Morph morph(MaterialShapes::circle(), MaterialShapes::softBurst());
            d->burst = m3shapes::toPath(morph, at, QRectF(-0.5, -0.5, 1, 1));
            d->burstAt = at;
        }
        const double since = std::fmod(t, 1.1);
        const double beat = since < 0.3 ? std::sin(M_PI * since / 0.3) : 0;
        const double size = (2 * r + 6) * critical * (1 + 0.14 * beat);
        p.save();
        p.translate(lead);
        p.rotate(t * 40);
        p.scale(size, size);
        p.setBrush(s.error);
        p.drawPath(d->burst);
        p.restore();
        p.setOpacity(clamp01(critical));
        p.setFont(e >= 0.5 ? d->openSymbol : d->closedSymbol);
        p.setPen(s.onError);
        p.drawText(QRectF(lead.x() - h / 2, cy - h / 2, h, h), Qt::AlignCenter, QStringLiteral("power"));
        p.setOpacity(1);
    }

    // Charging: the bolt in the ring
    const double bolt = clamp01(d->bolt->currentValue().toDouble());
    if (bolt > 0.01) {
        p.setOpacity(bolt);
        p.setFont(e >= 0.5 ? d->openSymbol : d->closedSymbol);
        p.setPen(s.primary);
        p.drawText(QRectF(lead.x() - h / 2, cy - h / 2, h, h), Qt::AlignCenter, QStringLiteral("bolt"));
        p.setOpacity(1);
    }
}

QString BatteryChunk::name() {
    return QStringLiteral("Power");
}

int BatteryChunk::expandedHeight() {
    return Open + 8;
}

int BatteryChunk::statusBarHeight() {
    return Closed;
}
