/****************************************
 *
 *   theDesk - live horizon view
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "horizonview.h"

#include <m3motion.h>

#include "horizonscene.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>
#include "horizonshapes.h"

namespace {
    constexpr int frameMs = 16;     // while animating
    // Desktop switch: the M3 expressive slow spatial spring
    // (computed on first use: they read settings, so not at library load)
    int shiftDuration() {
        static const int ms = M3Motion::duration(M3Motion::SpatialSlow);
        return ms;
    }

    double shiftEasing(double t) {
        static const QEasingCurve curve = M3Motion::easing(M3Motion::SpatialSlow);
        return curve.valueForProgress(qBound(0.0, t, 1.0));
    }
    constexpr int flareMs = 1100;   // rim flare
    constexpr int introMs = 1800;   // planet rising
    constexpr int idleMs = 60000;   // time-of-day refresh
    constexpr int sunsetMs = 800;   // fits in the 1 s sleep delay
    constexpr int sunriseMs = 2600;
    constexpr int captionMs = 1400;

    double easeOutCubic(double t) {
        t = qBound(0.0, t, 1.0);
        return 1 - qPow(1 - t, 3);
    }

    double easeInOut(double t) {
        t = qBound(0.0, t, 1.0);
        return t * t * (3 - 2 * t);
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
            a.greenF() + (b.greenF() - a.greenF()) * t,
            a.blueF() + (b.blueF() - a.blueF()) * t);
    }

    const QString upowerService = QStringLiteral("org.freedesktop.UPower");
    const QString displayDevicePath = QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice");
} // namespace

struct HorizonViewPrivate {
        int fixedScene = -1;

        // Cached layers at the current size
        QSize renderedSize;
        int margin = 0;
        HorizonScene::Geometry geometry;
        QMap<int, QPixmap> sky;
        QMap<int, QPixmap> planet;

        // Desktop position (horizontal shift of the apex)
        int desktopIndex = 0;
        int desktopCount = 1;
        double shiftFrom = 0, shiftTo = 0;
        qint64 shiftStart = -1;

        // Rim flares (lights travelling along the rim)
        struct Flare {
                double fromX, toX;
                qint64 start;
                int duration;
        };
        QList<Flare> flares;

        // Night amount (0 = time of day, 1 = night) for sunset/sunrise
        double nightFrom = 0, nightTo = 0;
        qint64 nightStart = -1;
        int nightMs = 1;

        // Notifications shown as moons rising from behind the planet
        struct Moon {
                QString title, subtitle;
                QPixmap icon;
                int slot;           // position along the horizon
                qint64 start;       // rise start
                qint64 setStart = -1; // set (sink) start
        };
        QList<Moon> moons;

        QString captionTitle, captionSubtitle;
        qint64 captionStart = -1;
        bool topShade = false;
        QRectF quiet;
        double tide = -1;

        // Intro (planet rising)
        qint64 introStart = -1;

        double battery = 1;
        bool charging = false;

        QElapsedTimer clock;
        QTimer frameTimer;
        QTimer idleTimer;

        double shiftFor(int index, int count, double width) const {
            if (count <= 1) return 0;
            double step = qMin(width * 0.07, margin * 2.0 / (count - 1));
            return (index - (count - 1) / 2.0) * step;
        }

        double currentNight(qint64 now) const {
            if (nightStart < 0) return nightTo;
            return nightFrom + (nightTo - nightFrom) * easeInOut(double(now - nightStart) / nightMs);
        }

        double currentShift(qint64 now) const {
            if (shiftStart < 0) return shiftTo;
            return shiftFrom + (shiftTo - shiftFrom) * shiftEasing(double(now - shiftStart) / shiftDuration());
        }
};

HorizonView::HorizonView(QWidget* parent) :
    QWidget(parent) {
    d = new HorizonViewPrivate();
    d->clock.start();
    this->setAttribute(Qt::WA_OpaquePaintEvent);

    d->frameTimer.setInterval(frameMs);
    connect(&d->frameTimer, &QTimer::timeout, this, [this] {
        this->update();
    });

    d->idleTimer.setInterval(idleMs);
    connect(&d->idleTimer, &QTimer::timeout, this, [this] {
        this->update(); // time of day moved on
    });
    d->idleTimer.start();

    QDBusConnection::systemBus().connect(upowerService, displayDevicePath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), this, SLOT(pollBattery()));
    pollBattery();
}

HorizonView::~HorizonView() {
    delete d;
}

void HorizonView::setFixedScene(int scene) {
    d->fixedScene = scene;
    this->update();
}

void HorizonView::setDesktop(int index, int count, bool animate) {
    const qint64 now = d->clock.elapsed();
    ensureRendered();
    double target = d->shiftFor(index, count, this->width());
    d->desktopIndex = index;
    d->desktopCount = count;

    if (!animate || !this->isVisible() || qFuzzyCompare(target + 1, d->currentShift(now) + 1)) {
        d->shiftFrom = d->shiftTo = target;
        d->shiftStart = -1;
        this->update();
        return;
    }

    // Slide the planet and send a flare along the rim to the new apex
    d->shiftFrom = d->currentShift(now);
    d->shiftTo = target;
    d->shiftStart = now;
    d->flares.append({this->width() / 2.0 + d->shiftFrom, this->width() / 2.0 + target, now, flareMs});
    startAnimating();
}

void HorizonView::setBattery(double level, bool charging) {
    level = qBound(0.0, level, 1.0);
    if (charging && !d->charging && this->isVisible()) {
        // Charger plugged in: one flare across the whole rim
        d->flares.append({-0.1 * this->width(), 1.1 * this->width(), d->clock.elapsed(), flareMs});
        startAnimating();
    }
    d->battery = level;
    d->charging = charging;
    this->update();
}

void HorizonView::playIntro() {
    d->introStart = d->clock.elapsed();
    startAnimating();
}

void HorizonView::playSunset() {
    const qint64 now = d->clock.elapsed();
    d->nightFrom = d->currentNight(now);
    d->nightTo = 1;
    d->nightStart = now;
    d->nightMs = sunsetMs;
    // The light withdraws from both edges into the apex
    double apex = this->width() / 2.0 + d->currentShift(now);
    d->flares.append({-0.05 * this->width(), apex, now, sunsetMs});
    d->flares.append({1.05 * this->width(), apex, now, sunsetMs});
    startAnimating();
}

void HorizonView::playSunrise() {
    const qint64 now = d->clock.elapsed();
    d->nightFrom = d->currentNight(now);
    d->nightTo = 0;
    d->nightStart = now;
    d->nightMs = sunriseMs;
    // Light spreads out from the apex
    double apex = this->width() / 2.0 + d->currentShift(now);
    d->flares.append({apex, -0.15 * this->width(), now + 300, sunriseMs});
    d->flares.append({apex, 1.15 * this->width(), now + 300, sunriseMs});
    startAnimating();
}

bool HorizonView::isNight() const {
    return d->nightTo > 0.5;
}

void HorizonView::setCaption(QString title, QString subtitle) {
    d->captionTitle = title;
    d->captionSubtitle = subtitle;
    d->captionStart = title.isEmpty() ? -1 : d->clock.elapsed();
    startAnimating();
}

void HorizonView::setQuietRect(QRectF rect) {
    d->quiet = rect;
    this->update();
}

void HorizonView::setTopShade(bool shade) {
    d->topShade = shade;
    this->update();
}

double HorizonView::batteryLevel() const {
    return d->battery;
}

void HorizonView::pollBattery() {
    QDBusMessage message = QDBusMessage::createMethodCall(upowerService, displayDevicePath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message.setArguments({QStringLiteral("org.freedesktop.UPower.Device")});
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* watcher) {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (!reply.isValid()) return;
        QVariantMap props = reply.value();
        if (props.value(QStringLiteral("Type")).toUInt() != 2) return; // no battery: keep full glow
        uint state = props.value(QStringLiteral("State")).toUInt();
        // 1 charging, 4 fully charged, 5 pending charge
        setBattery(props.value(QStringLiteral("Percentage")).toDouble() / 100.0, state == 1 || state == 4 || state == 5);
    });
}

void HorizonView::startAnimating() {
    if (!d->frameTimer.isActive()) d->frameTimer.start();
    this->update();
}

void HorizonView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    d->sky.clear();
    d->planet.clear();
    d->renderedSize = QSize();
    ensureRendered();
    d->shiftFrom = d->shiftTo = d->shiftFor(d->desktopIndex, d->desktopCount, this->width());
}

void HorizonView::ensureRendered() {
    if (d->renderedSize == this->size()) return;
    d->sky.clear();
    d->planet.clear();
    d->renderedSize = this->size();
    d->margin = int(this->width() * 0.4);
    d->geometry = HorizonScene::geometryFor(this->size());
}

void HorizonView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    ensureRendered();
    if (this->size().isEmpty()) return;

    const qint64 now = d->clock.elapsed();
    const double w = this->width(), h = this->height();
    const qreal dpr = this->devicePixelRatioF();

    auto skyFor = [this, dpr](int scene) -> const QPixmap& {
        if (!d->sky.contains(scene)) d->sky.insert(scene, QPixmap::fromImage(HorizonScene::render(scene, this->size(), dpr, HorizonScene::Sky)));
        return d->sky[scene];
    };
    auto planetFor = [this, dpr](int scene) -> const QPixmap& {
        if (!d->planet.contains(scene)) d->planet.insert(scene, QPixmap::fromImage(HorizonScene::renderPlanet(scene, this->size(), d->margin, dpr)));
        return d->planet[scene];
    };

    HorizonScene::Blend blend;
    if (d->fixedScene >= 0) {
        blend.from = blend.to = d->fixedScene;
    } else {
        blend = HorizonScene::blendForTime(QTime::currentTime());
    }

    // Animation state
    bool animating = false;
    double shift = d->currentShift(now);
    double push = 1;
    if (d->shiftStart >= 0) {
        double t = double(now - d->shiftStart) / shiftDuration();
        if (t >= 1) {
            d->shiftStart = -1;
        } else {
            push = 1 + 0.03 * qSin(M_PI * t); // gentle camera push-in
            animating = true;
        }
    }

    double rise = 0, skyOpacity = 1;
    if (d->introStart >= 0) {
        double t = double(now - d->introStart) / introMs;
        if (t >= 1) {
            d->introStart = -1;
        } else {
            rise = (1 - easeOutCubic(t)) * h * 0.45;
            skyOpacity = easeInOut(t * 1.6);
            animating = true;
        }
    }

    double night = d->currentNight(now);
    if (d->nightStart >= 0) {
        if (now - d->nightStart >= d->nightMs) {
            d->nightStart = -1;
        } else {
            animating = true;
        }
    }
    rise += night * h * 0.05; // the planet sinks a little at night

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform, push != 1);
    p.fillRect(this->rect(), Qt::black);

    // Current horizon geometry (shifted, pushed, risen)
    HorizonScene::Geometry g = d->geometry;
    g.radius *= push;
    g.centre = QPointF(w / 2 + shift, d->geometry.horizonY + rise + g.radius);

    // Scenes to composite, with their opacity
    QList<QPair<int, double>> layers = {{blend.from, 1}};
    if (blend.to != blend.from) layers.append({blend.to, blend.progress});
    if (night > 0 && !(blend.from == HorizonScene::AuroraNight && blend.to == HorizonScene::AuroraNight)) {
        layers.append({HorizonScene::AuroraNight, night});
    }

    // 1. Skies
    for (const auto& layer : layers) {
        if (layer.second <= 0) continue;
        p.setOpacity(layer.second * skyOpacity);
        p.drawPixmap(0, 0, skyFor(layer.first));
    }
    p.setOpacity(1);

    // 1b. M3E shapes, live so they drift and turn with desktop switches,
    // spring in with the planet and morph through the day
    if (HorizonShapes::enabled()) {
        double intro = d->introStart >= 0 ? qBound(0.0, double(now - d->introStart) / introMs, 1.0) : 1.0;
        HorizonShapes::paint(&p, blend.from, blend.to, blend.progress, this->size(), shift, intro, 1, d->quiet);
        if (night > 0 && !(blend.from == HorizonScene::AuroraNight && blend.to == HorizonScene::AuroraNight)) {
            HorizonShapes::paint(&p, HorizonScene::AuroraNight, HorizonScene::AuroraNight, 0, this->size(), shift, intro, night, d->quiet);
        }
    }

    // 2. Moons (notifications) between sky and planet, so they rise from behind it
    if (paintMoons(p, g, now, false)) animating = true;

    // 3. Planets
    for (const auto& layer : layers) {
        if (layer.second <= 0) continue;
        p.setOpacity(layer.second);
        const QPixmap& planet = planetFor(layer.first);
        p.save();
        // Apex of the planet layer is at its horizontal centre, y = horizonY
        p.translate(w / 2 + shift, d->geometry.horizonY + rise);
        p.scale(push, push);
        p.translate(-(planet.width() / dpr) / 2, -d->geometry.horizonY);
        p.drawPixmap(0, 0, planet);
        p.restore();
    }
    p.setOpacity(1);

    // Rim, drawn live: its glow follows the battery level
    QPainterPath rim;
    bool first = true;
    for (double x = -8; x <= w + 8; x += 8) {
        QPointF pt(x, g.rimY(x));
        if (first) {
            rim.moveTo(pt);
            first = false;
        } else {
            rim.lineTo(pt);
        }
    }
    double progress = blend.from == blend.to ? 0 : blend.progress;
    QColor rimColour = mix(HorizonScene::rimColour(blend.from), HorizonScene::rimColour(blend.to), progress);
    QColor glowColour = mix(HorizonScene::glowColour(blend.from), HorizonScene::glowColour(blend.to), progress);
    if (night > 0) {
        rimColour = mix(rimColour, HorizonScene::rimColour(HorizonScene::AuroraNight), night);
        glowColour = mix(glowColour, HorizonScene::glowColour(HorizonScene::AuroraNight), night);
    }
    double strength = 0.25 + 0.75 * d->battery;
    const double scale = h / 768.0;
    p.setBrush(Qt::NoBrush);
    glowColour.setAlphaF(0.22 * strength);
    p.setPen(QPen(glowColour, 10 * scale, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(rim);
    rimColour.setAlphaF(0.35 + 0.5 * strength);
    p.setPen(QPen(rimColour, 1.6 * scale));
    p.drawPath(rim);

    // Flares: lights travelling along the rim
    for (auto it = d->flares.begin(); it != d->flares.end();) {
        const auto& flare = *it;
        double t = double(now - flare.start) / flare.duration;
        if (t >= 1) {
            it = d->flares.erase(it);
            continue;
        }
        animating = true;
        if (t < 0) {
            ++it;
            continue; // delayed start
        }
        {
            double x = flare.fromX + (flare.toX - flare.fromX) * easeInOut(t);
            double intensity = qSin(M_PI * t);
            double half = w * 0.12;
            QPainterPath sweep;
            bool started = false;
            for (double sx = x - half; sx <= x + half; sx += 4) {
                QPointF pt(sx, g.rimY(sx));
                if (!started) {
                    sweep.moveTo(pt);
                    started = true;
                } else {
                    sweep.lineTo(pt);
                }
            }
            QLinearGradient light(x - half, 0, x + half, 0);
            light.setColorAt(0, QColor(255, 248, 230, 0));
            light.setColorAt(0.5, QColor(255, 248, 230, int(235 * intensity)));
            light.setColorAt(1, QColor(255, 248, 230, 0));
            QLinearGradient glow = light;
            glow.setColorAt(0.5, QColor(255, 236, 200, int(80 * intensity)));
            p.setPen(QPen(QBrush(glow), 12 * scale, Qt::SolidLine, Qt::RoundCap));
            p.drawPath(sweep);
            p.setPen(QPen(QBrush(light), 2.4 * scale, Qt::SolidLine, Qt::RoundCap));
            p.drawPath(sweep);
        }
        ++it;
    }

    // Focus tide: the rim is lit from the left edge up to the tide's position
    if (d->tide >= 0) {
        double tideX = d->tide * w;
        QPainterPath lit;
        bool firstLit = true;
        for (double x = 0; x <= tideX; x += 6) {
            QPointF pt(x, g.rimY(x));
            if (firstLit) {
                lit.moveTo(pt);
                firstLit = false;
            } else {
                lit.lineTo(pt);
            }
        }
        lit.lineTo(tideX, g.rimY(tideX));
        QLinearGradient tideGradient(0, 0, tideX, 0);
        tideGradient.setColorAt(0, QColor(0xfe, 0x80, 0x19, 40));
        tideGradient.setColorAt(1, QColor(0xfa, 0xbd, 0x2f, 210));
        p.setPen(QPen(QBrush(tideGradient), 2.6 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(lit);
        QPointF head(tideX, g.rimY(tideX));
        QRadialGradient headGlow(head, 14 * scale);
        headGlow.setColorAt(0, QColor(0xfb, 0xf1, 0xc7, 230));
        headGlow.setColorAt(1, QColor(0xfa, 0xbd, 0x2f, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(headGlow);
        p.drawEllipse(head, 14 * scale, 14 * scale);
        p.setBrush(Qt::NoBrush);
    }

    // Moon labels on top of everything else in the scene
    paintMoons(p, g, now, true);

    // Shade behind text at the top (lock screen clock)
    if (d->topShade) {
        QLinearGradient shade(0, 0, 0, h * 0.45);
        shade.setColorAt(0, QColor(0, 0, 0, 150));
        shade.setColorAt(1, QColor(0, 0, 0, 0));
        p.fillRect(QRectF(0, 0, w, h * 0.45), shade);
    }

    // Caption: serif title (and a quieter line below) fading in above the horizon
    if (d->captionStart >= 0 && !d->captionTitle.isEmpty()) {
        double t = double(now - d->captionStart) / captionMs;
        if (t < 1) animating = true;
        double opacity = easeInOut(t);
        double lift = (1 - easeOutCubic(t)) * h * 0.015;

        QFont title(QStringLiteral("Noto Serif"));
        title.setStyleHint(QFont::Serif);
        title.setWeight(QFont::Light);
        title.setPixelSize(qMax(22, int(h * 0.05)));
        QRectF titleRect(0, g.horizonY - h * 0.2 + lift, w, h * 0.09);
        p.setOpacity(opacity);
        p.setFont(title);
        p.setPen(QColor(0, 0, 0, 70));
        p.drawText(titleRect.translated(0, 2), Qt::AlignCenter, d->captionTitle);
        p.setPen(QColor(0xfb, 0xf1, 0xc7));
        p.drawText(titleRect, Qt::AlignCenter, d->captionTitle);

        if (!d->captionSubtitle.isEmpty()) {
            QFont subtitle = this->font();
            subtitle.setPixelSize(qMax(12, int(h * 0.02)));
            p.setFont(subtitle);
            p.setPen(QColor(0xeb, 0xdb, 0xb2, 210));
            p.drawText(QRectF(0, titleRect.bottom() + lift, w, h * 0.05), Qt::AlignHCenter | Qt::AlignTop, d->captionSubtitle);
        }
        p.setOpacity(1);
    }

    if (!animating) d->frameTimer.stop();
}

namespace {
    constexpr int moonRiseMs = 1700;
    constexpr int moonSetMs = 900;
    constexpr int maxMoons = 5;
    // Slots fan out from the apex: centre, left, right, further left, ...
    const int moonSlots[maxMoons] = {0, -1, 1, -2, 2};
} // namespace

void HorizonView::addMoon(QString title, QString subtitle, QIcon icon) {
    const qint64 now = d->clock.elapsed();
    // Set the oldest one down when the sky is full
    int visible = 0;
    for (const auto& moon : d->moons) {
        if (moon.setStart < 0) visible++;
    }
    if (visible >= maxMoons) {
        for (auto& moon : d->moons) {
            if (moon.setStart < 0) {
                moon.setStart = now;
                break;
            }
        }
    }

    // First free slot
    int slot = 0;
    for (int candidate : moonSlots) {
        bool used = false;
        for (const auto& moon : d->moons) {
            if (moon.setStart < 0 && moon.slot == candidate) used = true;
        }
        if (!used) {
            slot = candidate;
            break;
        }
    }

    int size = qMax(16, int(this->height() * 0.026));
    d->moons.append({title, subtitle, icon.pixmap(QSize(size, size), this->devicePixelRatioF()), slot, now});
    startAnimating();
}

void HorizonView::clearMoons() {
    const qint64 now = d->clock.elapsed();
    for (auto& moon : d->moons) {
        if (moon.setStart < 0) moon.setStart = now;
    }
    startAnimating();
}

int HorizonView::moonCount() const {
    int count = 0;
    for (const auto& moon : d->moons) {
        if (moon.setStart < 0) count++;
    }
    return count;
}

bool HorizonView::paintMoons(QPainter& p, const HorizonScene::Geometry& g, qint64 now, bool labels) {
    bool animating = false;
    const double w = this->width(), h = this->height();
    const double radius = h * 0.024;

    for (auto it = d->moons.begin(); it != d->moons.end();) {
        double rise = easeOutCubic(double(now - it->start) / moonRiseMs);
        double set = it->setStart >= 0 ? easeInOut(double(now - it->setStart) / moonSetMs) : 0;
        if (rise < 1 || (it->setStart >= 0 && set < 1)) animating = true;
        if (it->setStart >= 0 && set >= 1) {
            if (labels) it = d->moons.erase(it); // remove after both passes
            else ++it;
            continue;
        }

        double x = g.centre.x() + it->slot * w * 0.17;
        double lift = (h * 0.13 - qAbs(it->slot) * h * 0.012) * rise * (1 - set);
        QPointF centre(x, g.rimY(x) + radius * 1.2 - lift);
        double opacity = qMin(1.0, rise * 2) * (1 - set);

        p.save();
        p.setOpacity(opacity);
        if (!labels) {
            // Glow
            QRadialGradient glow(centre, radius * 3.2);
            glow.setColorAt(0, QColor(0xfb, 0xf1, 0xc7, 90));
            glow.setColorAt(1, QColor(0xfb, 0xf1, 0xc7, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(centre, radius * 3.2, radius * 3.2);
            // Disc, lit from the top-left
            QRadialGradient disc(centre - QPointF(radius * 0.4, radius * 0.4), radius * 1.6);
            disc.setColorAt(0, QColor(0xfb, 0xf1, 0xc7));
            disc.setColorAt(1, QColor(0xbd, 0xae, 0x93));
            p.setBrush(disc);
            p.drawEllipse(centre, radius, radius);
            if (!it->icon.isNull()) {
                QSizeF iconSize = QSizeF(it->icon.size()) / it->icon.devicePixelRatio();
                p.drawPixmap(QRectF(centre - QPointF(iconSize.width() / 2, iconSize.height() / 2), iconSize), it->icon, QRectF(it->icon.rect()));
            }
        } else {
            // Label beside the moon
            QFont title(QStringLiteral("Noto Serif"));
            title.setStyleHint(QFont::Serif);
            title.setPixelSize(qMax(12, int(h * 0.02)));
            QFontMetricsF titleMetrics(title);
            QString titleText = titleMetrics.elidedText(it->title, Qt::ElideRight, w * 0.14);
            QPointF textPos(centre.x() + radius * 1.6, centre.y() - radius * 0.1);
            p.setFont(title);
            p.setPen(QColor(0xfb, 0xf1, 0xc7));
            p.drawText(textPos, titleText);
            if (!it->subtitle.isEmpty()) {
                QFont sub = this->font();
                sub.setPixelSize(qMax(10, int(h * 0.015)));
                p.setFont(sub);
                p.setPen(QColor(0xd5, 0xc4, 0xa1, 200));
                p.drawText(textPos + QPointF(0, titleMetrics.height() * 0.9), QFontMetricsF(sub).elidedText(it->subtitle, Qt::ElideRight, w * 0.14));
            }
        }
        p.restore();
        ++it;
    }
    return animating;
}

void HorizonView::setTide(double progress) {
    d->tide = progress < 0 ? -1 : qBound(0.0, progress, 1.0);
    this->update();
}
