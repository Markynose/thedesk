/****************************************
 *
 *   theDesk - the charging ripple
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "chargingripple.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimer>
#include <QtMath>
#include <compositor.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <Wm/desktopwm.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <tsettings.h>

namespace {
    // A spring stepped by the ripple's own clock
    struct Spring {
            double x = 0, v = 0, target = 0;
            M3Motion::Spring spec = M3Motion::spring(M3Motion::SpatialDefault);

            void step(double dt) {
                const double k = spec.stiffness, c = 2 * spec.dampingRatio * std::sqrt(k);
                const int n = qMax(1, int(std::ceil(dt / 0.002)));
                const double h = dt / n;
                for (int i = 0; i < n; i++) {
                    v += (-k * (x - target) - c * v) * h;
                    x += v * h;
                }
            }
    };

    struct Sparkle {
            double angle, depth, size, twinkle;
    };

    constexpr double RippleSeconds = 2.1;
    constexpr double SecondRipple = 0.16;
    constexpr double FlyAt = 2.45;
    constexpr double DoneAt = 3.9;

    double clamp01(double t) {
        return qBound(0.0, t, 1.0);
    }

    // Fast at first, settling as it reaches the far side
    double decelerate(double t) {
        return 1 - std::pow(1 - clamp01(t), 2.2);
    }

    // A circle whose edge is a wave of a constant wavelength
    QPainterPath wavyCircle(QPointF centre, double radius, double amplitude, double wavelength, double phase) {
        QPainterPath path;
        const int steps = qMax(64, int(2 * M_PI * radius / 3));
        const double waves = qMax(3.0, std::round(2 * M_PI * radius / wavelength));
        for (int i = 0; i <= steps; i++) {
            const double a = 2 * M_PI * i / steps;
            const QPointF point = centre + QPointF(std::cos(a), std::sin(a)) * (radius + amplitude * std::sin(waves * a + phase));
            if (i == 0) path.moveTo(point);
            else path.lineTo(point);
        }
        path.closeSubpath();
        return path;
    }

    // Part of a wavy circle, clockwise from the top (degrees)
    QPainterPath wavyArc(QPointF centre, double radius, double to, double amplitude, double wavelength, double phase) {
        QPainterPath path;
        const double waves = std::round(2 * M_PI * radius / wavelength);
        const int steps = qMax(8, int(to * 2));
        for (int i = 0; i <= steps; i++) {
            const double degrees = to * i / steps;
            const double a = qDegreesToRadians(degrees - 90);
            const QPointF point = centre + QPointF(std::cos(a), std::sin(a)) * (radius + amplitude * std::sin(waves * qDegreesToRadians(degrees) + phase));
            if (i == 0) path.moveTo(point);
            else path.lineTo(point);
        }
        return path;
    }

    // Where the charger goes in, in a screen of this size (Power/chargingRipple.from)
    QPointF chargerAt(const QSizeF& size) {
        tSettings settings;
        const QString from = settings.value(QStringLiteral("Power/chargingRipple.from")).toString();
        const QRectF r(QPointF(0, 0), size);
        return from == QStringLiteral("bottom")         ? QPointF(r.center().x(), r.bottom())
               : from == QStringLiteral("bottom-right") ? r.bottomRight()
               : from == QStringLiteral("left")         ? QPointF(r.left(), r.center().y())
               : from == QStringLiteral("right")        ? QPointF(r.right(), r.center().y())
                                                        : r.bottomLeft();
    }

    QPointer<ChargingRipple> playing;
    QPointer<ChargerCorner> calling;
} // namespace

struct ChargingRipplePrivate {
        int percentage;
        QString detail;
        QPointer<QWidget> target;
        QPointF origin;
        std::vector<Sparkle> sparkles;

        QTimer* ticker;
        QElapsedTimer clock;
        double time = 0;
        double landedAt = -1;

        Spring badge, fly, level, text, weight, pulse;
};

bool ChargingRipple::play(int percentage, qint64 secondsToFull, QWidget* target) {
    tSettings settings;
    if (!settings.value(QStringLiteral("Power/chargingRipple")).toBool()) return false;
    // It needs translucency, and the calmer motion scheme goes without
    if (!Compositor::isActive() || !M3Motion::expressive()) return false;
    if (playing) return true;
    QScreen* screen = target && target->isVisible() && target->screen() ? target->screen() : QGuiApplication::primaryScreen();
    if (!screen) return false;
    playing = new ChargingRipple(percentage, secondsToFull, target, screen);
    return true;
}

ChargingRipple::ChargingRipple(int percentage, qint64 secondsToFull, QWidget* target, QScreen* screen) :
    QWidget(nullptr) {
    d = new ChargingRipplePrivate();
    d->percentage = qBound(0, percentage, 100);
    d->target = target;
    if (secondsToFull > 0) {
        const qint64 minutes = (secondsToFull + 59) / 60;
        d->detail = minutes >= 60 ? tr("Charging · full in %1 h %2 min").arg(minutes / 60).arg(minutes % 60) : tr("Charging · full in %1 min").arg(minutes);
    } else {
        d->detail = tr("Charging");
    }

    // Over everything, never in the way
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool | Qt::WindowTransparentForInput);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setWindowTitle(QStringLiteral("theDesk charging"));
    this->setGeometry(screen->geometry());

    const QRectF r(QPointF(0, 0), screen->geometry().size());
    d->origin = chargerAt(r.size());
    // Sparkles across the directions the rings travel, mostly near the edge
    const double toward = qRadiansToDegrees(std::atan2(r.center().y() - d->origin.y(), r.center().x() - d->origin.x()));
    QRandomGenerator* random = QRandomGenerator::global();
    for (int i = 0; i < 260; i++) {
        d->sparkles.push_back({toward - 90 + random->generateDouble() * 180, std::pow(random->generateDouble(), 1.8), 1.4 + random->generateDouble() * 2, random->generateDouble() * 6.28});
    }

    d->badge.spec = M3Motion::spring(M3Motion::SpatialDefault);
    d->fly.spec = M3Motion::spring(M3Motion::SpatialDefault);
    d->level.spec = {1.0, 120}; // the level fills over about a second, no overshoot
    d->text.spec = M3Motion::spring(M3Motion::EffectsDefault);
    d->weight.spec = M3Motion::spring(M3Motion::SpatialDefault);
    d->weight.x = d->weight.target = 400;
    d->pulse.spec = M3Motion::spring(M3Motion::EffectsDefault);

    d->ticker = new QTimer(this);
    d->ticker->setInterval(16);
    connect(d->ticker, &QTimer::timeout, this, &ChargingRipple::tick);
    d->clock.start();
    d->ticker->start();
    this->show();
}

ChargingRipple::~ChargingRipple() {
    delete d;
}

void ChargingRipple::tick() {
    const double dt = qMin(0.05, d->clock.restart() / 1000.0);
    d->time += dt;
    const double t = d->time;

    if (t >= 0.25) d->badge.target = 1;
    if (t >= 0.40) d->level.target = d->percentage / 100.0;
    if (t >= 0.50) {
        d->text.target = 1;
        d->weight.target = 720;
    }
    if (t >= FlyAt) {
        // Into the bar's battery, if it's showing; else it just goes
        const bool landing = d->target && d->target->isVisible();
        d->fly.target = landing ? 1 : 0;
        d->badge.target = landing ? 0.18 : 0;
        d->text.target = 0;
    }
    if (d->landedAt < 0 && d->fly.x > 0.9) d->landedAt = t;
    d->pulse.target = d->landedAt >= 0 && t < d->landedAt + 0.3 ? 1 : 0;

    for (Spring* spring : {&d->badge, &d->fly, &d->level, &d->text, &d->weight, &d->pulse}) spring->step(dt);
    this->update();

    if (t > DoneAt) {
        d->ticker->stop();
        this->close();
        this->deleteLater();
    }
}

void ChargingRipple::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const double t = d->time;
    const double reach = std::hypot(this->width(), this->height()) * 1.08;

    // The rings, a beat apart
    for (int ring = 0; ring < 2; ring++) {
        const double k = (t - ring * SecondRipple) / RippleSeconds;
        if (k <= 0 || k >= 1) continue;
        const double radius = 30 + decelerate(k) * reach;
        const double fade = (1 - k) * (1 - k) * (ring ? 0.6 : 1.0);
        const double band = 70 + 50 * k;
        const QPainterPath edge = wavyCircle(d->origin, radius, 5, 44, -t * 6);

        // A soft glow behind the edge
        QRadialGradient glow(d->origin, radius + 6);
        QColor clear = s.primary, soft = s.primary, strong = s.primary;
        clear.setAlphaF(0);
        soft.setAlphaF(float(0.30 * fade));
        strong.setAlphaF(float(0.55 * fade));
        glow.setColorAt(0, clear);
        glow.setColorAt(qMax(0.0, (radius - band) / (radius + 6)), clear);
        glow.setColorAt(0.97, soft);
        glow.setColorAt(1, strong);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawPath(edge);

        // The wavy edge
        QColor line = s.primary.lighter(125);
        line.setAlphaF(float(0.85 * fade));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(line, 2.5));
        p.drawPath(edge);

        // Sparkles riding inside the first
        if (ring == 0) {
            p.setPen(Qt::NoPen);
            for (const Sparkle& sparkle : d->sparkles) {
                const double a = qDegreesToRadians(sparkle.angle);
                const QPointF at = d->origin + QPointF(std::cos(a), std::sin(a)) * (radius - sparkle.depth * band);
                if (!this->rect().contains(at.toPoint())) continue;
                const double twinkle = 0.5 + 0.5 * std::sin(sparkle.twinkle + t * 14);
                QColor colour = s.onPrimaryContainer;
                colour.setAlphaF(float(qMin(1.0, fade * 1.3) * twinkle * (1 - sparkle.depth)));
                p.setBrush(colour);
                p.drawEllipse(at, sparkle.size * 0.6, sparkle.size * 0.6);
            }
        }
    }

    // Where it lands: the bar's battery icon
    QPointF landing = QPointF(this->width() / 2.0, 24);
    QRectF chunk;
    if (d->target && d->target->isVisible()) {
        const QPoint topLeft = this->mapFromGlobal(d->target->mapToGlobal(QPoint(0, 0)));
        chunk = QRectF(topLeft, d->target->size());
        landing = QPointF(chunk.left() + chunk.height() / 2, chunk.center().y());
    }
    if (d->pulse.x > 0.01 && !chunk.isEmpty()) {
        QColor pulse = s.primary;
        pulse.setAlphaF(float(0.22 * clamp01(d->pulse.x)));
        p.setPen(Qt::NoPen);
        p.setBrush(pulse);
        p.drawRoundedRect(chunk, chunk.height() / 2, chunk.height() / 2);
    }

    // The badge: a turning shape with a bolt, the level around it
    const double scale = qMax(0.0, d->badge.x);
    if (scale < 0.01 || d->fly.x > 0.995) return;
    const QPointF middle(this->width() / 2.0, this->height() / 2.0);
    const QPointF at = middle * (1 - d->fly.x) + landing * d->fly.x;
    const double size = 132 * scale;
    QTransform turn;
    turn.translate(at.x(), at.y());
    turn.rotate(t * 24);
    p.setPen(Qt::NoPen);
    p.setBrush(s.primaryContainer);
    p.drawPath(turn.map(m3shapes::toPath(MaterialShapes::softBurst(), QRectF(-size / 2, -size / 2, size, size))));
    QFont symbol(QStringLiteral("Material Symbols Rounded"));
    symbol.setPixelSize(qMax(1, int(size * 0.42)));
    symbol.setVariableAxis(QFont::Tag("FILL"), 1);
    symbol.setVariableAxis(QFont::Tag("wght"), 500);
    symbol.setVariableAxis(QFont::Tag("opsz"), 48);
    p.setFont(symbol);
    p.setPen(s.onPrimaryContainer);
    p.drawText(QRectF(at.x() - size, at.y() - size, 2 * size, 2 * size), Qt::AlignCenter, QStringLiteral("bolt"));

    // Wavy circular progress: the level, then the track after a gap
    const double ring = size / 2 + 22 * scale;
    const double filled = 360 * clamp01(d->level.x);
    const double away = 1 - d->fly.x;
    QColor track = s.secondaryContainer, active = s.primary;
    track.setAlphaF(float(away));
    active.setAlphaF(float(away));
    const double gap = qRadiansToDegrees(10 / qMax(1.0, ring));
    if (filled + 2 * gap < 360) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(track, 5 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(at.x() - ring, at.y() - ring, 2 * ring, 2 * ring), int((90 - filled - gap) * 16), int(-(360 - filled - 2 * gap) * 16));
    }
    if (filled > 1) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(active, 5 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // The wave drifts back toward the start
        p.drawPath(wavyArc(at, ring, filled, 3 * scale, 32, t * 5));
    }

    // The level, in expressive type, under it
    const double shown = clamp01(d->text.x) * away;
    if (shown > 0.01) {
        QFont big(QStringLiteral("Roboto Flex"));
        big.setPixelSize(56);
        big.setVariableAxis(QFont::Tag("wght"), float(d->weight.x));
        big.setVariableAxis(QFont::Tag("wdth"), 110);
        big.setVariableAxis(QFont::Tag("opsz"), 56);
        p.setOpacity(shown);
        p.setFont(big);
        p.setPen(s.onSurface);
        const double y = at.y() + ring + 14 + (1 - clamp01(d->text.x)) * 16;
        p.drawText(QRectF(at.x() - 200, y, 400, 64), Qt::AlignCenter, QLocale().toString(qRound(clamp01(d->level.x) * 100)) + QStringLiteral("%"));
        QFont detail(QStringLiteral("Roboto Flex"));
        detail.setPixelSize(16);
        p.setFont(detail);
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(at.x() - 240, y + 62, 480, 24), Qt::AlignCenter, d->detail);
        p.setOpacity(1);
    }
}

struct ChargerCorner::Private {
        QPointF origin, symbol; // the corner and the plug, in the window
        bool composited = false;
        bool answered = false;  // the charger went in
        bool leaving = false;

        QTimer ticker;
        QElapsedTimer clock;
        double time = 0;
        Spring grow, answer;

        QPainterPath shape; // the cookie (to a circle as it answers) at shapeAt, in a unit square
        float shapeAt = -1;
        M3::Scheme scheme = M3::currentScheme();
        QFont glyphs;
};

namespace {
    constexpr double CornerSize = 168, Reach = 196;
    constexpr double PingFrom = 0.3, PingEvery = 1.1;
    constexpr int Pings = 3;
} // namespace

void ChargerCorner::follow(bool critical, bool charging) {
    if (critical && !calling) {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) calling = new ChargerCorner(screen);
        return;
    }
    if (!calling || critical || calling->d->leaving) return;
    // Plugged in: the bolt first, then it goes; else it just goes
    calling->d->leaving = true;
    if (!calling->d->composited) {
        calling->close();
        calling->deleteLater();
        return;
    }
    calling->d->answered = charging;
    if (charging) calling->d->answer.target = 1;
    else calling->d->grow.target = 0;
    calling->d->time = qMax(calling->d->time, PingFrom + Pings * PingEvery); // no more pings
    calling->d->clock.restart();
    calling->d->ticker.start();
}

ChargerCorner::ChargerCorner(QScreen* screen) :
    QWidget(nullptr) {
    d = new Private();
    d->composited = Compositor::isActive();
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool | Qt::WindowTransparentForInput);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setWindowTitle(QStringLiteral("theDesk charger"));

    // A square around the corner, cut to the screen
    const QRect g = screen->geometry();
    const QPointF at = chargerAt(g.size());
    const QRect area = QRectF(at.x() - Reach, at.y() - Reach, 2 * Reach, 2 * Reach).toAlignedRect() & QRect(QPoint(0, 0), g.size());
    this->setGeometry(area.translated(g.topLeft()));
    d->origin = at - area.topLeft();
    d->symbol = d->origin + QPointF(at.x() <= 0 ? 32 : at.x() >= g.width() ? -32 : 0, at.y() <= 0 ? 32 : at.y() >= g.height() ? -32 : 0);

    d->glyphs = QFont(QStringLiteral("Material Symbols Rounded"));
    d->glyphs.setPixelSize(28);
    d->glyphs.setVariableAxis(QFont::Tag("FILL"), 1);
    d->glyphs.setVariableAxis(QFont::Tag("wght"), 500);
    d->glyphs.setVariableAxis(QFont::Tag("opsz"), 28);
    d->grow.spec = M3Motion::spring(M3Motion::SpatialDefault);
    d->answer.spec = M3Motion::spring(M3Motion::EffectsDefault);
    d->grow.target = 1;
    if (d->composited) {
        d->ticker.setInterval(33);
        connect(&d->ticker, &QTimer::timeout, this, &ChargerCorner::tick);
        d->clock.start();
        d->ticker.start();
    } else {
        // Still and whole: the mask is set once
        d->grow.x = 1;
        const QPainterPath cookie = m3shapes::toPath(MaterialShapes::cookie9Sided(), QRectF(d->origin.x() - CornerSize / 2, d->origin.y() - CornerSize / 2, CornerSize, CornerSize));
        this->setMask(QRegion(cookie.toFillPolygon().toPolygon()));
    }

    connect(DesktopWm::instance(), &DesktopWm::activeWindowChanged, this, &ChargerCorner::updateCovered);
    updateCovered();
}

ChargerCorner::~ChargerCorner() {
    delete d;
}

void ChargerCorner::updateCovered() {
    // Behind a fullscreen window, as the bar is
    DesktopWmWindowPtr active = DesktopWm::activeWindow();
    const bool covered = active && active->isFullScreen() && !active->isMinimized() && active->isOnCurrentDesktop();
    this->setVisible(!covered);
}

void ChargerCorner::tick() {
    const double dt = qMin(0.05, d->clock.restart() / 1000.0);
    d->time += dt;
    if (d->answered && d->answer.x > 0.9) d->grow.target = 0; // the bolt shown, it goes as the ripple spreads
    for (Spring* spring : {&d->grow, &d->answer}) spring->step(dt);
    this->update();

    const bool pinging = !d->leaving && M3Motion::expressive() && d->time < PingFrom + Pings * PingEvery;
    const bool settled = std::abs(d->grow.x - d->grow.target) < 0.005 && std::abs(d->grow.v) < 0.05 && std::abs(d->answer.x - d->answer.target) < 0.005 && std::abs(d->answer.v) < 0.05;
    if (pinging || !settled) return;
    d->ticker.stop(); // rests until the charger goes in
    if (d->leaving) {
        this->close();
        this->deleteLater();
    }
}

void ChargerCorner::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme& s = d->scheme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const bool moving = d->composited && M3Motion::expressive();
    const double t = d->time, grow = qMax(0.0, d->grow.x), answer = clamp01(d->answer.x);

    // A wavy ping from the corner, beating the shape as it sets off
    double beat = 0;
    const double since = t - PingFrom;
    if (moving && !d->leaving && since > 0 && since < Pings * PingEvery) {
        const double k = std::fmod(since, PingEvery) / PingEvery;
        QColor ring = s.error;
        ring.setAlphaF(float(0.85 * (1 - k)));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ring, 2.5));
        p.drawPath(wavyCircle(d->origin, 96 + 90 * decelerate(k), 3, 30, -t * 5));
        p.setPen(Qt::NoPen);
        const double inBeat = std::fmod(since, PingEvery);
        beat = inBeat < 0.3 ? std::sin(M_PI * inBeat / 0.3) : 0;
    }

    // The cookie, turning; a circle once it answers
    if (answer != d->shapeAt) {
        static const m3shapes::Morph morph(MaterialShapes::cookie9Sided(), MaterialShapes::circle());
        d->shape = m3shapes::toPath(morph, float(answer), QRectF(-0.5, -0.5, 1, 1));
        d->shapeAt = float(answer);
    }
    const double size = CornerSize * grow * (1 + 0.05 * beat);
    p.save();
    p.translate(d->origin);
    if (moving) p.rotate(t * 14);
    p.scale(size, size);
    QColor fill = s.errorContainer;
    const QColor to = s.primary;
    fill = QColor::fromRgbF(float(fill.redF() + (to.redF() - fill.redF()) * answer), float(fill.greenF() + (to.greenF() - fill.greenF()) * answer), float(fill.blueF() + (to.blueF() - fill.blueF()) * answer));
    p.setBrush(fill);
    p.drawPath(d->shape);
    p.restore();

    // The plug, the bolt
    p.setFont(d->glyphs);
    const QRectF box(d->symbol.x() - 28, d->symbol.y() - 28, 56, 56);
    p.setOpacity(clamp01(grow) * (1 - answer));
    p.setPen(s.onErrorContainer);
    p.drawText(box, Qt::AlignCenter, QStringLiteral("power"));
    p.setOpacity(clamp01(grow) * answer);
    p.setPen(s.onPrimary);
    p.drawText(box, Qt::AlignCenter, QStringLiteral("bolt"));
}
