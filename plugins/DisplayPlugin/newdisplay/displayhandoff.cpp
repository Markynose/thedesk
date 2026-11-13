/****************************************
 *
 *   theDesk - a chosen display mode travels to the display
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "displayhandoff.h"

#include "displaywaitingscreen.h"
#include <QElapsedTimer>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <cmath>
#include <compositor.h>
#include <m3scheme.h>

namespace {
    constexpr int GatherMs = 220;   // the tile draws in to a drop
    constexpr double Radius = 20;   // the drop
    constexpr int Tail = 5;         // drops that follow it, each smaller
    constexpr double TailLag = 0.045; // how far behind each is, of the way

    double smooth(double x) {
        x = qBound(0.0, x, 1.0);
        return x * x * (3 - 2 * x);
    }

    // The drop with its tail: circles along the way already travelled,
    // joined, so it stretches as it speeds up and gathers as it arrives
    QPainterPath dropAlong(const QPointF& from, const QPointF& to, double progress) {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        QPointF before;
        double radiusBefore = 0;
        for (int i = 0; i < Tail; i++) {
            const double at = smooth(progress - i * TailLag * (1 - progress * 0.4));
            const QPointF centre = from + (to - from) * at;
            const double radius = Radius * (1 - 0.17 * i);
            QPainterPath circle;
            circle.addEllipse(centre, radius, radius);
            path = path.united(circle);
            if (i > 0) {
                const QLineF line(before, centre);
                if (line.length() > 0.5) {
                    const QLineF normal = line.normalVector().unitVector();
                    const QPointF n(normal.dx(), normal.dy());
                    QPolygonF bridge;
                    bridge << before + n * radiusBefore << centre + n * radius << centre - n * radius << before - n * radiusBefore;
                    QPainterPath join;
                    join.addPolygon(bridge);
                    join.closeSubpath();
                    path = path.united(join);
                }
            }
            before = centre;
            radiusBefore = radius;
        }
        return path;
    }

    // On the main screen, over everything, taking no clicks
    class DropWindow : public QWidget {
        public:
            explicit DropWindow(QScreen* screen) :
                QWidget(nullptr) {
                this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
                this->setAttribute(Qt::WA_TranslucentBackground);
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                this->setWindowTitle(QStringLiteral("theDesk display drop"));
                this->setScreen(screen);
                this->setGeometry(screen->geometry());
            }

            void setDrops(const QPainterPath& global) {
                drops = global.translated(-QPointF(this->geometry().topLeft()));
                // No compositor: the window is cut to the drops themselves
                if (!Compositor::isActive()) {
                    const QRegion region = QRegion(drops.toFillPolygon().toPolygon()).intersected(this->rect());
                    this->setMask(region.isEmpty() ? QRegion(-2, -2, 1, 1) : region);
                }
                this->update();
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.fillPath(drops, M3::currentScheme().primary);
            }

        private:
            QPainterPath drops;
    };
} // namespace

struct DisplayHandoffPrivate {
        QRectF tile;
        QPointer<DropWindow> window;
        QList<QPointer<DisplayWaitingScreen>> targets;
        QTimer* timer;
        QElapsedTimer clock;
        int travelMs = 800;
        bool done = false;
};

DisplayHandoff::DisplayHandoff(const QRect& tile, QScreen* mainScreen, const QList<DisplayWaitingScreen*>& targets, QObject* parent) :
    QObject(parent) {
    d = new DisplayHandoffPrivate();
    d->tile = QRectF(tile);
    for (DisplayWaitingScreen* target : targets) d->targets.append(target);
    d->window = new DropWindow(mainScreen);

    // The farther the display, the longer the way: all arrive together
    double farthest = 0;
    for (DisplayWaitingScreen* target : targets) farthest = qMax(farthest, QLineF(d->tile.center(), target->shapeCentre()).length());
    d->travelMs = qBound(650, int(farthest / 2.0), 1050);

    d->timer = new QTimer(this);
    d->timer->setInterval(16);
    d->timer->setTimerType(Qt::PreciseTimer);
    connect(d->timer, &QTimer::timeout, this, &DisplayHandoff::frame);
}

DisplayHandoff::~DisplayHandoff() {
    if (d->window) d->window->deleteLater();
    delete d;
}

void DisplayHandoff::start() {
    d->clock.start();
    d->window->show();
    d->window->raise();
    d->timer->start();
    this->frame();
}

void DisplayHandoff::frame() {
    if (d->done) return;
    const qint64 ms = d->clock.elapsed();
    QPainterPath all;
    all.setFillRule(Qt::WindingFill);
    if (ms < GatherMs) {
        // The tile draws in to a drop where it stood
        const double k = smooth(ms / double(GatherMs));
        const QPointF centre = d->tile.center();
        const double w = d->tile.width() + (2 * Radius - d->tile.width()) * k;
        const double h = d->tile.height() + (2 * Radius - d->tile.height()) * k;
        const double corner = 16 + (Radius - 16) * k;
        all.addRoundedRect(QRectF(centre.x() - w / 2, centre.y() - h / 2, w, h), corner, corner);
    } else {
        const double progress = qMin(1.0, (ms - GatherMs) / double(d->travelMs));
        for (const QPointer<DisplayWaitingScreen>& target : std::as_const(d->targets)) {
            if (!target) continue;
            const QPainterPath drop = dropAlong(d->tile.center(), target->shapeCentre(), progress);
            target->setDrop(drop);
            all = all.united(drop);
        }
        if (progress >= 1) {
            d->done = true;
            d->timer->stop();
            if (d->window) d->window->hide();
            emit landed();
            this->deleteLater();
            return;
        }
    }
    if (d->window) d->window->setDrops(all);
}
