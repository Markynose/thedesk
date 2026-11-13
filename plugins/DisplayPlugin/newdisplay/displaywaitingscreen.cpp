/****************************************
 *
 *   theDesk - what a new display shows until it's set up
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "displaywaitingscreen.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRegion>
#include <QTimer>
#include <QScreen>
#include <QVariantAnimation>
#include <cmath>
#include <livingtypeview.h>
#include <m3scheme.h>
#include <memory>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <vector>

namespace {
    // Spring step response from rest to 1 (the shape's morphs; one frame
    // clock drives everything here, as in the loading screen)
    double spring(double t, double zeta, double stiffness) {
        if (t <= 0) return 0;
        const double w = std::sqrt(stiffness), wd = w * std::sqrt(1 - zeta * zeta);
        return 1 - std::exp(-zeta * w * t) * (std::cos(wd * t) + zeta * w / wd * std::sin(wd * t));
    }

    double ease(double x) {
        x = qBound(0.0, x, 1.0);
        return 1 - std::pow(1 - x, 3);
    }

    const m3shapes::RoundedPolygon& shapeNamed(const char* name) {
        for (const auto& named : MaterialShapes::all()) {
            if (qstrcmp(named.name, name) == 0) return named.shape();
        }
        return MaterialShapes::circle();
    }

    QFont flex(double px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    constexpr const char* Cycle[] = {"Cookie9Sided", "Clover4Leaf", "SoftBurst", "Cookie12Sided", "Sunny", "Cookie7Sided"};
    constexpr int CycleCount = int(std::size(Cycle));
} // namespace

// The calm middle over the field: the shape, the device, the words
class DisplayWaitingOverlay : public QWidget {
    public:
        DisplayWaitingOverlay(bool laptop, DisplayWaitingScreen::Kind kind, QWidget* parent) :
            QWidget(parent), laptop(laptop), kind(kind) {
            this->setAttribute(Qt::WA_TransparentForMouseEvents);
            for (int i = 0; i < CycleCount; i++) morphs.push_back(std::make_unique<m3shapes::Morph>(shapeNamed(Cycle[i]), shapeNamed(Cycle[(i + 1) % CycleCount])));
            clock.start();
            // No timer of its own while waiting: the field under it repaints
            // the whole window every frame, and this with it. Alone (a twin
            // or a cover) it keeps its own time
            if (kind != DisplayWaitingScreen::Waiting) {
                auto* frame = new QTimer(this);
                frame->setInterval(16);
                QObject::connect(frame, &QTimer::timeout, this, [this] {
                    this->update();
                });
                frame->start();
            }
            // A cover stands where a landed shape stood before the displays changed
            if (kind == DisplayWaitingScreen::Cover) landedAt = -100000;
        }

        double opacity = 1;
        QElapsedTimer clock;
        QPainterPath drop;          // on its way here, in this widget's coordinates
        qint64 landedAt = -1;       // when it arrived (ms on the clock)
        bool isOpen = false;        // the shape is a hole now: nothing to draw

        double seconds() const {
            // A cover's shape is already there, mid-turn
            return clock.elapsed() / 1000.0 + (kind == DisplayWaitingScreen::Cover ? 20 : 0);
        }
        double unit() const {
            return qBound(0.6, qMin(this->width() / 1280.0, this->height() / 720.0), 2.0);
        }
        QPointF centre() const {
            return QPointF(this->width() / 2.0, this->height() / 2.0 - (kind == DisplayWaitingScreen::Waiting ? 40 * unit() : 0));
        }
        double sinceLanded() const {
            return landedAt == -1 ? -1 : (clock.elapsed() - landedAt) / 1000.0;
        }
        // The shape as it is now (its morph, its turn), `side` across
        QPainterPath shapeNow(double side) const {
            const double t = seconds();
            const double period = 2.4;
            const int step = int(std::floor(qMax(0.0, t - 1.0) / period));
            const double local = qMax(0.0, t - 1.0) - step * period;
            const double k = t < 1.0 ? 0 : spring(local, 0.6, 140);
            QPainterPath path = m3shapes::toPath(*morphs[step % CycleCount], float(qBound(0.0, k, 1.0)), QRectF(-side / 2, -side / 2, side, side));
            QTransform transform;
            transform.translate(centre().x(), centre().y());
            transform.rotate(step * 90 + k * 90 + t * 6);
            return transform.map(path);
        }
        // How big it stands: in with a spring, a little bigger once the drop is in it
        double sideNow() const {
            // (a twin's grows out of the drop that stayed)
            const double landed = sinceLanded();
            const double in = kind == DisplayWaitingScreen::Cover ? 1 : kind == DisplayWaitingScreen::Twin ? spring(landed, 0.55, 120) : spring(seconds() - 0.3, 0.55, 120);
            const double swell = landed < 0 ? 0 : kind == DisplayWaitingScreen::Cover ? 1 : spring(landed, 0.5, 300);
            return 210 * unit() * in * (1 + 0.16 * swell);
        }

    protected:
        void paintEvent(QPaintEvent*) override {
            const double t = seconds();
            const M3::Scheme s = M3::currentScheme();
            const double unit = this->unit();
            const QPointF c = centre();
            const double landed = sinceLanded();
            const double arrived = landed < 0 ? 0 : ease(landed / 0.2);
            const double words = kind == DisplayWaitingScreen::Waiting ? 1 - arrived : 0;

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setRenderHint(QPainter::TextAntialiasing);
            p.setOpacity(opacity);

            // The pool: the field fades out under the middle (drawn once)
            if (kind == DisplayWaitingScreen::Waiting) {
                if (pool.size() != this->size()) {
                    pool = QPixmap(this->size());
                    pool.fill(Qt::transparent);
                    QPainter pp(&pool);
                    const QPointF centre = c + QPointF(0, 70 * unit);
                    QRadialGradient gradient(centre, 420 * unit);
                    QColor solid = s.surfaceContainerLowest, clear = solid;
                    clear.setAlphaF(0);
                    gradient.setColorAt(0, solid);
                    gradient.setColorAt(0.55, solid);
                    gradient.setColorAt(1, clear);
                    pp.translate(centre);
                    pp.scale(1.25, 0.72);
                    pp.translate(-centre);
                    pp.fillRect(QRectF(-this->width(), -this->height(), 3 * this->width(), 3 * this->height()), gradient);
                }
                p.drawPixmap(0, 0, pool);
            }
            if (isOpen) return;

            // The drop, on its way in: it ends under the shape
            if (!drop.isEmpty()) p.fillPath(drop, s.primary);

            // The shape: in with a spring, then a morph every 2.4 s, a
            // quarter turn each, and a slow drift; the drop's colour once
            // it has it
            p.fillPath(shapeNow(sideNow()), M3::layer(s.primaryContainer, s.primary, arrived));

            // The device: a still symbol (no pulse), fading in
            if (words > 0) {
                QFont symbol(QStringLiteral("Material Symbols Rounded"));
                symbol.setPixelSize(qRound(88 * unit));
                symbol.setVariableAxis(QFont::Tag("FILL"), 1);
                symbol.setVariableAxis(QFont::Tag("wght"), 500);
                symbol.setVariableAxis(QFont::Tag("opsz"), 48);
                QColor iconColour = s.onPrimaryContainer;
                iconColour.setAlphaF(float(ease((t - 0.5) / 0.4) * words));
                p.setFont(symbol);
                p.setPen(iconColour);
                p.drawText(QRectF(c.x() - 100 * unit, c.y() - 100 * unit, 200 * unit, 200 * unit), Qt::AlignCenter, laptop ? QStringLiteral("laptop") : QStringLiteral("computer"));

                auto line = [&](const QString& text, double y, const QFont& font, QColor colour, double at) {
                    const double k = ease((t - at) / 0.5);
                    if (k <= 0) return;
                    colour.setAlphaF(float(k * words));
                    p.setFont(font);
                    p.setPen(colour);
                    p.drawText(QRectF(0, y - 40 * unit + (1 - k) * 18 * unit, this->width(), 80 * unit), Qt::AlignCenter, text);
                };
                line(QCoreApplication::translate("DisplayWaitingScreen", "Choose how to use this display"), c.y() + 190 * unit, flex(40 * unit, 600), s.onSurface, 0.7);
                line(laptop ? QCoreApplication::translate("DisplayWaitingScreen", "Mirror or extend it from the laptop") : QCoreApplication::translate("DisplayWaitingScreen", "Mirror or extend it from the main screen"), c.y() + 238 * unit, flex(22 * unit, 400), s.onSurfaceVariant, 0.9);
            }
        }

    private:
        bool laptop;
        DisplayWaitingScreen::Kind kind;
        QPixmap pool;
        std::vector<std::unique_ptr<m3shapes::Morph>> morphs;
};

struct DisplayWaitingScreenPrivate {
        LivingTypeView* field = nullptr;
        DisplayWaitingOverlay* overlay;
        DisplayWaitingScreen::Kind kind;
        QPointer<QScreen> screen;
        bool dismissing = false;
        // A twin's ground fades in under its shape
        QElapsedTimer shown;
        // Opening: the shape is a hole that grows
        QTimer* iris = nullptr;
        QElapsedTimer irisClock;
};

DisplayWaitingScreen::DisplayWaitingScreen(QScreen* screen, bool laptop, Kind kind, QWidget* parent) :
    QWidget(parent) {
    d = new DisplayWaitingScreenPrivate();
    d->kind = kind;
    d->screen = screen;
    d->shown.start();
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
    this->setAttribute(Qt::WA_TranslucentBackground); // the fade out shows the desktop under it
    this->setWindowTitle(QStringLiteral("theDesk new display"));
    this->setScreen(screen);
    this->setGeometry(screen->geometry());

    // The loading screen's field with the display's own words: nothing
    // personal, it could be a projector
    if (kind == Waiting) {
        d->field = new LivingTypeView(this);
        d->field->setMode(LivingTypeView::Field);
        d->field->setFarewell({tr("Waiting..."), QStringLiteral("TheDesk"), tr("Mirror or extend"), laptop ? tr("Choose on the laptop") : tr("Choose on the main screen")});
    }
    d->overlay = new DisplayWaitingOverlay(laptop, kind, this);

    // A display that goes away takes this with it; one that changes (the
    // displays being set up as chosen) keeps it
    connect(screen, &QObject::destroyed, this, &QObject::deleteLater);
    connect(screen, &QScreen::geometryChanged, this, [this](const QRect& geometry) {
        this->setGeometry(geometry);
    });
}

DisplayWaitingScreen::~DisplayWaitingScreen() {
    delete d;
}

QScreen* DisplayWaitingScreen::shownOn() const {
    return d->screen;
}

QPointF DisplayWaitingScreen::shapeCentre() const {
    return QPointF(this->geometry().topLeft()) + d->overlay->centre();
}

void DisplayWaitingScreen::setDrop(const QPainterPath& drop) {
    d->overlay->drop = drop.translated(-QPointF(this->geometry().topLeft()));
    d->overlay->update();
}

void DisplayWaitingScreen::land() {
    if (d->overlay->landedAt != -1) return;
    d->overlay->landedAt = d->overlay->clock.elapsed();
    d->overlay->drop = QPainterPath();
    d->overlay->update();
}

void DisplayWaitingScreen::open() {
    if (d->dismissing || d->iris) return;
    d->dismissing = true;
    // The shape itself becomes the hole and grows past the farthest corner,
    // deep dips and all. A window mask, so it works without a compositor
    const QPointF centre = d->overlay->centre();
    double farthest = 0;
    for (const QPointF& corner : {QPointF(0, 0), QPointF(this->width(), 0), QPointF(0, this->height()), QPointF(this->width(), this->height())}) {
        farthest = qMax(farthest, QLineF(centre, corner).length());
    }
    const double from = d->overlay->sideNow();
    const double reach = farthest * 4.6;
    static const QEasingCurve accelerate = [] {
        QEasingCurve c(QEasingCurve::BezierSpline); // M3 "emphasized accelerate"
        c.addCubicBezierSegment(QPointF(0.3, 0), QPointF(0.8, 0.15), QPointF(1, 1));
        return c;
    }();
    d->overlay->isOpen = true;
    d->iris = new QTimer(this);
    d->iris->setInterval(16);
    d->iris->setTimerType(Qt::PreciseTimer);
    d->irisClock.start();
    connect(d->iris, &QTimer::timeout, this, [this, from, reach] {
        const double progress = qMin(1.0, d->irisClock.elapsed() / 700.0);
        const double side = from + (reach - from) * accelerate.valueForProgress(progress);
        const QRegion mask = QRegion(this->rect()).subtracted(QRegion(d->overlay->shapeNow(side).toFillPolygon().toPolygon()));
        // An empty region means "no mask" to Qt: keep one pixel outside instead
        this->setMask(mask.isEmpty() ? QRegion(-2, -2, 1, 1) : mask);
        if (progress >= 1) {
            d->iris->stop();
            this->hide();
            this->deleteLater();
        }
    });
    d->iris->start();
}

void DisplayWaitingScreen::dismiss() {
    if (d->dismissing) return;
    d->dismissing = true;
    auto* fade = new QVariantAnimation(this);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setDuration(250);
    connect(fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        this->setWindowOpacity(value.toDouble());
    });
    connect(fade, &QVariantAnimation::finished, this, &QObject::deleteLater);
    fade->start();
}

void DisplayWaitingScreen::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    QColor ground = M3::currentScheme().surfaceContainerLowest;
    // A twin comes in over the desktop the choice was made on
    if (d->kind == Twin) {
        ground.setAlphaF(float(ease(d->shown.elapsed() / 250.0)));
        if (ground.alphaF() < 1) QTimer::singleShot(16, this, qOverload<>(&QWidget::update));
    }
    p.fillRect(this->rect(), ground);
}

void DisplayWaitingScreen::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event)
    if (d->field) d->field->setGeometry(this->rect());
    d->overlay->setGeometry(this->rect());
}
