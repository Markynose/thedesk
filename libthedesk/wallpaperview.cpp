/****************************************
 *
 *   theDesk - the shape wallpaper
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "wallpaperview.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>
#include <memory>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <tsettings.h>
#include <vector>

namespace {
    using Poly = m3shapes::RoundedPolygon;

    double c01(double x) {
        return qBound(0.0, x, 1.0);
    }

    double smooth(double a, double b, double x) {
        const double t = c01((x - a) / (b - a));
        return t * t * (3 - 2 * t);
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = c01(t);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t);
    }

    // One spring driven by the view's frame timer
    struct Spring {
            double v, vel = 0, t;
            M3Motion::Token token;

            Spring(double x, M3Motion::Token token) :
                v(x), t(x), token(token) {}

            void snap(double x) {
                v = t = x;
                vel = 0;
            }

            bool step(double dt) { // true while it moves
                if (v == t && vel == 0) return false;
                const auto s = M3Motion::stepSpring(v, vel, t, M3Motion::spring(token), dt);
                v = s.value;
                vel = s.velocity;
                if (qAbs(v - t) < 1e-3 && qAbs(vel) < 1e-2) {
                    snap(t);
                    return false;
                }
                return true;
            }
    };

    struct Cell {
            QPointF at; // at 768 px high, before the pan
            int kind;   // 0, 1: the picked shapes; 2: the accent
            int role;
            double turn, dir;
            double popAt = -1, sweepAt = -1;
            Spring pop{1, M3Motion::SpatialFast};
            Spring kick{0, M3Motion::SpatialDefault}; // degrees
            Spring colour{0, M3Motion::EffectsDefault};
            Spring shape{0, M3Motion::SpatialDefault};
    };

    // The wallpaper is dark whatever the mode (the user's choice)
    M3::Scheme darkScheme() {
        M3::Scheme s = M3::currentScheme();
        if (!s.dark) {
            const M3::Config config = M3::Config::fromSettings();
            s = M3::Scheme::generate(s.seed, true, config.variant, config.contrast);
        }
        return s;
    }

    QColor ground(const M3::Scheme& s) {
        return mix(s.surfaceContainerLowest, s.primaryContainer, 0.2);
    }

    QColor roleColour(const M3::Scheme& s, int kind, int role) {
        if (kind == 2) return s.tertiary;
        return role == 0 ? s.primaryContainer : role == 1 ? s.secondaryContainer : s.tertiaryContainer;
    }

    const Poly* shapeNamed(const QString& name) {
        for (const auto& named : MaterialShapes::all()) {
            if (name.trimmed().compare(QLatin1String(named.name), Qt::CaseInsensitive) == 0) return &named.shape();
        }
        return nullptr;
    }

    // At 768 px high: the grid's spacing, a shape's size, one desktop's pan
    constexpr double stepX = 132, stepY = 116, shapeSize = 118, panStep = 96;
    constexpr double accentScale = 0.42;
} // namespace

struct WallpaperViewPrivate {
        tSettings settings{QStringLiteral("theSuite"), QStringLiteral("theDesk")};
        std::vector<Cell> cells;
        int desktop = 0, desktopCount = 1;
        QSize builtFor;
        int builtCount = 0;

        Spring pan{0, M3Motion::SpatialDefault};
        Spring lock{0, M3Motion::SpatialSlow}; // 1: zoomed in and quiet behind the clock
        Spring dim{0, M3Motion::EffectsSlow};
        Spring grey{0, M3Motion::EffectsSlow};
        Spring ground{0, M3Motion::EffectsSlow}; // the sweep's ground colour
        QRectF quiet;

        // A sweep goes from these to the new scheme and shapes
        M3::Scheme from, to;
        const Poly* fromShapes[3] = {};
        const Poly* toShapes[3] = {};
        std::unique_ptr<m3shapes::Morph> morphs[3];
        QHash<int, QPainterPath> paths; // unit paths by kind and morph step
        bool sweeping = false;

        QElapsedTimer clock;
        qint64 last = 0;
        QTimer frame;
        QPixmap still;
        bool stillDirty = true;

        double now() const {
            return clock.elapsed() / 1000.0;
        }

        const QPainterPath& unitPath(int kind, double k) {
            const int step = fromShapes[kind] == toShapes[kind] ? 0 : qRound(c01(k) * 32);
            const int key = kind * 64 + step;
            auto it = paths.find(key);
            if (it != paths.end()) return *it;
            if (!morphs[kind]) morphs[kind] = std::make_unique<m3shapes::Morph>(*fromShapes[kind], *toShapes[kind]);
            return *paths.insert(key, m3shapes::toPath(*morphs[kind], float(step / 32.0), QRectF(-0.5, -0.5, 1, 1)));
        }

        void finishSweep() {
            from = to;
            for (int i = 0; i < 3; i++) {
                fromShapes[i] = toShapes[i];
                morphs[i].reset();
            }
            paths.clear();
            for (Cell& c : cells) {
                c.colour.snap(0);
                c.shape.snap(0);
                c.sweepAt = -1;
            }
            ground.snap(0);
            sweeping = false;
        }
};

WallpaperView::WallpaperView(QWidget* parent) :
    QWidget(parent) {
    d = new WallpaperViewPrivate();
    d->clock.start();
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    d->frame.setInterval(16);
    d->frame.setTimerType(Qt::PreciseTimer);
    connect(&d->frame, &QTimer::timeout, this, &WallpaperView::tick);

    d->from = d->to = darkScheme();
    const QStringList names = shapeNames();
    for (int i = 0; i < 2; i++) d->fromShapes[i] = d->toShapes[i] = shapeNamed(names.at(i));
    d->fromShapes[2] = d->toShapes[2] = &MaterialShapes::circle();

    connect(&d->settings, &tSettings::settingChanged, this, [this](QString key) {
        if (key == QStringLiteral("Wallpaper/shapes")) schemeOrShapesChanged();
    });
}

WallpaperView::~WallpaperView() {
    delete d;
}

QStringList WallpaperView::shapeNames() {
    tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
    // An unquoted "a,b" in the file comes back as a list, a quoted one as a string
    QStringList names = settings.value(QStringLiteral("Wallpaper/shapes")).toStringList().join(QLatin1Char(',')).split(QLatin1Char(','), Qt::SkipEmptyParts);
    const QStringList defaults = {QStringLiteral("Cookie9Sided"), QStringLiteral("Clover4Leaf")};
    for (int i = 0; i < 2; i++) {
        if (i >= names.size()) names.append(defaults.at(i));
        names[i] = names.at(i).trimmed();
        if (!shapeNamed(names.at(i))) names[i] = defaults.at(i);
    }
    return names.mid(0, 2);
}

void WallpaperView::setDesktop(int index, int count, bool animate) {
    count = qMax(1, count);
    const int old = d->desktop;
    d->desktop = index;
    if (count != d->desktopCount) {
        d->desktopCount = count;
        ensureCells(); // a wider grid
    }
    const double unit = this->height() / 768.0;
    d->pan.t = -index * panStep * unit;
    if (!animate || !this->isVisible()) {
        d->pan.snap(d->pan.t);
        d->stillDirty = true;
        this->update();
        return;
    }
    // A swipe jiggles the shapes
    if (index != old) {
        for (Cell& c : d->cells) c.kick.vel += 70 * c.dir * (index > old ? -1 : 1);
    }
    startAnimating();
}

void WallpaperView::setLook(Look look, bool animate) {
    d->lock.t = look == Look::Desktop ? 0 : 1;
    d->dim.t = look == Look::Desktop ? 0 : look == Look::Lock ? 0.34 : 0.5;
    d->grey.t = look == Look::Greeter ? 0.35 : 0;
    if (!animate) {
        for (Spring* s : {&d->lock, &d->dim, &d->grey}) s->snap(s->t);
        d->stillDirty = true;
        this->update();
        return;
    }
    startAnimating();
}

void WallpaperView::setQuietRect(QRectF rect) {
    d->quiet = rect;
    d->stillDirty = true;
    this->update();
}

void WallpaperView::playIntro() {
    ensureCells();
    for (Cell& c : d->cells) c.pop.snap(0);
    playPop();
}

void WallpaperView::playPop() {
    // From the middle outwards
    ensureCells();
    const double w = qMax(1, this->width()), h = qMax(1, this->height()), unit = h / 768.0, now = d->now();
    for (Cell& c : d->cells) {
        const QPointF s = c.at * unit + QPointF(d->pan.v, 0);
        c.popAt = now + 0.08 + 0.45 * std::hypot((s.x() - w / 2) / w, (s.y() - h / 2) / h);
    }
    startAnimating();
}

void WallpaperView::schemeOrShapesChanged() {
    const M3::Scheme scheme = darkScheme();
    const QStringList names = shapeNames();
    const Poly* shapes[2] = {shapeNamed(names.at(0)), shapeNamed(names.at(1))};
    if (scheme == d->to && shapes[0] == d->toShapes[0] && shapes[1] == d->toShapes[1]) return;
    ensureCells();
    if (d->sweeping) d->finishSweep();
    d->to = scheme;
    for (int i = 0; i < 2; i++) {
        if (d->toShapes[i] == shapes[i]) continue;
        d->toShapes[i] = shapes[i];
        d->morphs[i].reset();
    }
    d->paths.clear();
    if (!this->isVisible()) {
        d->finishSweep();
        d->stillDirty = true;
        this->update();
        return;
    }
    // A sweep from left to right: each shape turns, morphs and recolours
    d->sweeping = true;
    d->ground.t = 1;
    const double unit = this->height() / 768.0, now = d->now();
    for (Cell& c : d->cells) {
        c.sweepAt = now + 0.9 * c01((c.at.x() * unit + d->pan.v) / qMax(1, this->width()));
    }
    startAnimating();
}

void WallpaperView::startAnimating() {
    if (!d->frame.isActive()) {
        d->last = d->clock.elapsed();
        d->frame.start();
    }
    d->stillDirty = true;
    this->update();
}

void WallpaperView::tick() {
    const qint64 ms = d->clock.elapsed();
    const double dt = qBound(0.0, (ms - d->last) / 1000.0, 1 / 30.0);
    d->last = ms;
    const double now = ms / 1000.0;
    bool moving = false;
    for (Spring* s : {&d->pan, &d->lock, &d->dim, &d->grey, &d->ground}) moving |= s->step(dt);
    for (Cell& c : d->cells) {
        if (c.popAt >= 0) {
            moving = true;
            if (now >= c.popAt) {
                c.popAt = -1;
                if (c.pop.t == 0) {
                    c.pop.t = 1; // the intro: from nothing
                } else {
                    c.pop.vel += 2.4;
                    c.kick.vel += 80 * c.dir;
                }
            }
        }
        if (c.sweepAt >= 0) {
            moving = true;
            if (now >= c.sweepAt) {
                c.sweepAt = -1;
                c.colour.t = 1;
                c.shape.t = 1;
                c.kick.t += 45 * c.dir;
            }
        }
        for (Spring* s : {&c.pop, &c.kick, &c.colour, &c.shape}) moving |= s->step(dt);
    }
    if (!moving) {
        if (d->sweeping) d->finishSweep();
        d->frame.stop();
        d->stillDirty = true;
    }
    this->update();
}

void WallpaperView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ensureCells();
}

void WallpaperView::ensureCells() {
    // The grid, wide enough to pan across every desktop
    if (d->builtFor == this->size() && d->builtCount == d->desktopCount) return;
    d->builtFor = this->size();
    d->builtCount = d->desktopCount;
    if (d->sweeping) d->finishSweep();
    d->cells.clear();
    const double unit = qMax(1, this->height()) / 768.0;
    const int columns = qCeil((this->width() / unit + (d->desktopCount - 1) * panStep) / stepX) + 2;
    const int rows = qCeil(this->height() / unit / stepY) + 1;
    for (int j = -1; j <= rows; j++) {
        for (int i = -2; i <= columns; i++) {
            Cell c;
            c.at = QPointF(i * stepX + (j & 1 ? stepX / 2 : 0), j * stepY + 40);
            const unsigned h = unsigned((i + 7) * 73856093) ^ unsigned((j + 5) * 19349663);
            c.kind = h % 11 == 0 ? 2 : (i + j) & 1;
            c.role = int(h % 3);
            c.turn = (h % 4) * 22.5;
            c.dir = (i + j) & 1 ? 1 : -1;
            d->cells.push_back(c);
        }
    }
    d->pan.snap(-d->desktop * panStep * unit);
    d->stillDirty = true;
}

void WallpaperView::changeEvent(QEvent* event) {
    // The platform theme moved the scheme on (a new seed, the time of day)
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) schemeOrShapesChanged();
    QWidget::changeEvent(event);
}

void WallpaperView::paintEvent(QPaintEvent* event) {
    if (this->size().isEmpty()) return;
    if (d->frame.isActive()) {
        paintFrame(this);
        return;
    }
    const qreal dpr = this->devicePixelRatioF();
    if (d->stillDirty || d->still.size() != this->size() * dpr) {
        d->still = QPixmap(this->size() * dpr);
        d->still.setDevicePixelRatio(dpr);
        paintFrame(&d->still);
        d->stillDirty = false;
    }
    const QRect r = event->rect();
    QPainter p(this);
    p.drawPixmap(r, d->still, QRectF(QPointF(r.topLeft()) * dpr, QSizeF(r.size()) * dpr));
}

void WallpaperView::paintFrame(QPaintDevice* device) {
    ensureCells();
    const double w = this->width(), h = this->height(), unit = h / 768.0;
    const QPointF centre(w / 2, h / 2);
    const double zoom = 1 + 0.08 * d->lock.v, quietK = c01(d->lock.v);

    QPainter p(device);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(QRectF(0, 0, w, h), mix(ground(d->from), ground(d->to), d->ground.v));
    p.translate(centre);
    p.scale(zoom, zoom);
    p.translate(-centre);
    for (Cell& c : d->cells) {
        const QPointF s = c.at * unit + QPointF(d->pan.v, 0);
        if (s.x() < -140 * unit || s.x() > w + 140 * unit) continue;
        // Nothing behind the windows: the shapes shrink to nothing towards the middle
        const double dx = (s.x() - w / 2) / (w * 0.40), dy = (s.y() - h / 2) / (h * 0.52);
        double f = c01((smooth(0.62, 1.22, std::sqrt(dx * dx + dy * dy)) - 0.25) / 0.75) * (c.kind == 2 ? accentScale : 1);
        if (quietK > 0 && d->quiet.isValid()) {
            const QPointF z = centre + (s - centre) * zoom;
            const double qx = qMax(0.0, qMax(d->quiet.left() - z.x(), z.x() - d->quiet.right()));
            const double qy = qMax(0.0, qMax(d->quiet.top() - z.y(), z.y() - d->quiet.bottom()));
            f *= 1 - quietK * (1 - smooth(0, 160 * unit, std::hypot(qx, qy)));
        }
        const double size = shapeSize * unit * f * c.pop.v;
        if (size < 6 * unit) continue;
        QColor colour = mix(roleColour(d->from, c.kind, c.role), roleColour(d->to, c.kind, c.role), c.colour.v);
        if (d->grey.v > 0) colour = mix(colour, QColor::fromHslF(0, 0, colour.lightnessF() * 0.9), d->grey.v);
        p.save();
        p.translate(s);
        p.rotate(c.turn + c.kick.v);
        p.scale(size, size);
        p.fillPath(d->unitPath(c.kind, c.shape.v), colour);
        p.restore();
    }
    p.resetTransform();
    if (d->dim.v > 0.002) p.fillRect(QRectF(0, 0, w, h), QColor::fromRgbF(0, 0, 0, float(c01(d->dim.v))));
}
