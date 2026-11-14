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
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <m3circularprogress.h>
#include <m3motion.h>
#include <m3rollinglabel.h>
#include <m3scheme.h>
#include <powermanager.h>
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

    // A Material Symbol as a path centred on its own ink, not its font box
    QPainterPath glyph(const QString& name, int px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), 20);
        QPainterPath path;
        path.addText(0, 0, font, name);
        return path.translated(-path.boundingRect().center());
    }

    double clamp01(double t) {
        return qBound(0.0, t, 1.0);
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = clamp01(t);
        return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
            float(a.blueF() + (b.blueF() - a.blueF()) * t), float(a.alphaF() + (b.alphaF() - a.alphaF()) * t));
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
        const QPainterPath closedBolt = glyph(QStringLiteral("bolt"), 13), openBolt = glyph(QStringLiteral("bolt"), 15);
        const QPainterPath closedPlug = glyph(QStringLiteral("power"), 13), openPlug = glyph(QStringLiteral("power"), 15);
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
        qint64 clockAt = 0; // where the wave was when it last rested
        bool locked = false, screenOff = false, covered = false; // nobody sees it: it rests
        DesktopWmWindowPtr active;

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

    connect(StateManager::powerManager(), &PowerManager::lockedChanged, this, [this](bool locked) {
        d->locked = locked;
        updateDrift();
    });
    connect(DesktopWm::instance(), &DesktopWm::activeWindowChanged, this, &BatteryChunk::updateCovered);
    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, this, &BatteryChunk::updateCovered);

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

    updateDrift();
    this->update();
}

void BatteryChunk::setScreenOff(bool off) {
    d->screenOff = off;
    updateDrift();
}

void BatteryChunk::updateCovered() {
    // A fullscreen window in front (the bar steps behind it); its state can change while it stays active
    DesktopWmWindowPtr active = DesktopWm::activeWindow();
    if (active != d->active) {
        if (d->active) d->active->disconnect(this);
        d->active = active;
        if (active) connect(active, &DesktopWmWindow::windowStateChanged, this, &BatteryChunk::updateCovered);
    }
    d->covered = active && active->isFullScreen() && !active->isMinimized() && active->isOnCurrentDesktop();
    updateDrift();
}

void BatteryChunk::updateDrift() {
    // The wave drifts while low and seen; calmer motion leaves it still
    const bool moving = d->percentage <= 5 || (d->percentage > 10 && d->percentage <= 20); // the wave (flat at 10% and under) or the burst
    const bool run = d->onBattery && moving && M3Motion::expressive() && !d->locked && !d->screenOff && !d->covered;
    if (run == d->drift.isActive()) return;
    if (run) {
        d->clock.start();
        d->drift.start();
    } else {
        d->clockAt += d->clock.elapsed(); // it carries on from where it rested
        d->drift.stop();
    }
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

    const double t = (d->clockAt + (d->drift.isActive() ? d->clock.elapsed() : 0)) / 1000.0;
    // whole pixels: with the 3 px stroke the ring's edges land on pixel edges
    const QPointF lead(std::round(pill.left() + h / 2), std::round(cy));
    const double r = 8.5 + 2 * e;
    const QColor track = mix(mix(s.secondaryContainer, mix(s.errorContainer, s.onErrorContainer, 0.3), low), mix(s.primaryContainer, s.onPrimaryContainer, 0.3), plugged);
    const bool wavy = d->onBattery && d->percentage > 10 && d->percentage <= 20;
    // the wave drifts back toward the start, a wavelength a second
    M3CircularProgress::paint(p, lead, r, d->percentage / 100.0, mix(mix(s.primary, s.error, low), s.primary, plugged), track, wavy ? M3CircularProgress::WavyAmplitude : 0, t * 2 * M_PI);

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
        const double size = (2 * r + M3CircularProgress::Stroke + 1) * critical * (1 + 0.14 * beat);
        p.save();
        p.translate(lead);
        p.rotate(t * 40);
        p.scale(size, size);
        p.setBrush(s.error);
        p.drawPath(d->burst);
        p.restore();
        p.setOpacity(clamp01(critical));
        p.setBrush(s.onError);
        p.drawPath((e >= 0.5 ? d->openPlug : d->closedPlug).translated(lead));
        p.setOpacity(1);
    }

    // Charging: the bolt in the ring
    const double bolt = clamp01(d->bolt->currentValue().toDouble());
    if (bolt > 0.01) {
        p.setOpacity(bolt);
        p.setBrush(s.primary);
        p.drawPath((e >= 0.5 ? d->openBolt : d->closedBolt).translated(lead));
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
