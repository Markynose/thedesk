/****************************************
 *
 *   theDesk - onboarding: the opening
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "onboardingintro.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTimer>
#include <QtMath>
#include <cmath>
#include <optional>
#include <m3scheme.h>

namespace {
    double spring(double t, double d, double k) {
        if (t <= 0) return 0;
        const double w = std::sqrt(k);
        if (d >= 1) return 1 - std::exp(-w * t) * (1 + w * t);
        const double wd = w * std::sqrt(1 - d * d);
        return 1 - std::exp(-d * w * t) * (std::cos(wd * t) + d * w / wd * std::sin(wd * t));
    }
    double c01(double v) { return qBound(0.0, v, 1.0); }
    double smooth(double v) {
        v = c01(v);
        return v * v * (3 - 2 * v);
    }
    double easeInOut(double v) {
        v = c01(v);
        return v < 0.5 ? 4 * v * v * v : 1 - std::pow(-2 * v + 2, 3) / 2;
    }
    QColor mix(QColor a, QColor b, double k) {
        k = c01(k);
        return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * k), float(a.greenF() + (b.greenF() - a.greenF()) * k), float(a.blueF() + (b.blueF() - a.blueF()) * k));
    }
    QColor withAlpha(QColor c, double a) {
        c.setAlphaF(float(c01(a)));
        return c;
    }

    // Roboto Flex's axes (defaults at rest, a bold word)
    struct Axes {
            double wght = 700, wdth = 125, slnt = 0, grad = 0, xtra = 468, yopq = 79, ytas = 750, ytuc = 712;
    };

    // Timeline (s)
    constexpr double writeEnd = 0.9, crouch = 1.7, launch = 2.05, headArrive = 5.9, flowEnd = 7.0, fallStart = 7.2;
    constexpr double endAt = 9.6; // landed and breathed: the greeting takes over

    // The scribble: along the word's baseline, past the k, a big loop, a dip,
    // a swoop and off the right edge; sampled every 2 px
    struct Path {
            QList<QPointF> pts;
            void build(const QList<QPointF>& ctrl) {
                QList<QPointF> dense;
                for (int i = 1; i + 2 < ctrl.size(); i++) {
                    const QPointF p0 = ctrl[i - 1], p1 = ctrl[i], p2 = ctrl[i + 1], p3 = ctrl[i + 2];
                    for (double t = 0; t < 1; t += 0.001) {
                        const double t2 = t * t, t3 = t2 * t;
                        dense.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3));
                    }
                }
                // Exactly every 2 px along the curve (positions are looked up
                // by distance)
                pts = {dense.first()};
                double carried = 0;
                for (int i = 1; i < dense.size(); i++) {
                    QPointF from = dense[i - 1];
                    double segment = QLineF(from, dense[i]).length();
                    while (carried + segment >= 2) {
                        const double take = 2 - carried;
                        const QPointF point = from + (dense[i] - from) * (take / segment);
                        pts.append(point);
                        segment -= take;
                        from = point;
                        carried = 0;
                    }
                    carried += segment;
                }
            }
            double length() const { return (pts.size() - 1) * 2.0; }
            // position; tangent angle (deg); climb (-1 down .. 1 up); bend
            QPointF at(double d, double* angle = nullptr, double* climb = nullptr, double* bend = nullptr) const {
                if (pts.size() < 4) return {};
                if (d > length()) { // straight on past the end
                    const QPointF e = pts.last(), b = pts[pts.size() - 2];
                    const QPointF dir = (e - b) / qMax(0.01, QLineF(b, e).length());
                    if (angle) *angle = qRadiansToDegrees(std::atan2(dir.y(), dir.x()));
                    if (climb) *climb = -dir.y();
                    if (bend) *bend = 0;
                    return e + dir * (d - length());
                }
                const int i = qBound(1, int(qMax(0.0, d) / 2), int(pts.size()) - 3);
                const QPointF a = pts[i - 1], b = pts[i + 1];
                if (angle) *angle = qRadiansToDegrees(std::atan2(b.y() - a.y(), b.x() - a.x()));
                if (climb) *climb = -(b.y() - a.y()) / qMax(0.1, QLineF(a, b).length());
                if (bend) {
                    const int j = qBound(1, i + 6, int(pts.size()) - 2), k = qBound(1, i - 6, int(pts.size()) - 2);
                    const double a1 = std::atan2(pts[j + 1].y() - pts[j - 1].y(), pts[j + 1].x() - pts[j - 1].x());
                    const double a0 = std::atan2(pts[k + 1].y() - pts[k - 1].y(), pts[k + 1].x() - pts[k - 1].x());
                    *bend = std::abs(std::remainder(a1 - a0, 2 * M_PI)) / 24.0;
                }
                return pts[i];
            }
    };
} // namespace

struct OnboardingIntroPrivate {
        M3::Scheme scheme;
        QTimer frame;
        QElapsedTimer clock;
        qint64 offset = 0; // ms added to the clock (a skip moves it on)
        bool started = false, done = false;
        bool stays = false; // keeps the word at rest when it ends

        // Layout (built on resize)
        QString word = QStringLiteral("theDesk");
        double unit = 1; // the height against 768
        double px = 104, left = 44, baseY = 131;
        QList<double> restD; // each letter's centre along the path
        double kD = 0, kAdv = 60, headEnd = 0;
        Path path;

        // The outlines, cached by letter and (rounded) axes: drawing a frame
        // is then mostly filling paths
        QHash<QString, QPainterPath> glyphs;

        double now() const {
            return started ? (clock.elapsed() + offset) / 1000.0 : -1;
        }

        QFont font(const Axes& a) const {
            QFont f(QStringLiteral("Roboto Flex"));
            f.setPixelSize(qMax(1, int(px)));
            f.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
            f.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, a.wght, 1000.0)));
            f.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, a.wdth, 151.0)));
            f.setVariableAxis(QFont::Tag("slnt"), float(qBound(-10.0, a.slnt, 0.0)));
            f.setVariableAxis(QFont::Tag("GRAD"), float(qBound(-200.0, a.grad, 150.0)));
            f.setVariableAxis(QFont::Tag("XTRA"), float(qBound(323.0, a.xtra, 603.0)));
            f.setVariableAxis(QFont::Tag("YOPQ"), float(qBound(25.0, a.yopq, 135.0)));
            f.setVariableAxis(QFont::Tag("YTAS"), float(qBound(649.0, a.ytas, 854.0)));
            f.setVariableAxis(QFont::Tag("YTUC"), float(qBound(528.0, a.ytuc, 760.0)));
            return f;
        }

        // A letter's outline, centred on its advance, baseline at 0
        const QPainterPath& glyph(QChar ch, Axes a) {
            auto q = [](double v, double step) { return std::round(v / step) * step; };
            a.wght = q(a.wght, 20);
            a.wdth = q(a.wdth, 5);
            a.slnt = q(a.slnt, 1);
            a.grad = q(a.grad, 20);
            a.xtra = q(a.xtra, 15);
            a.yopq = q(a.yopq, 6);
            a.ytas = q(a.ytas, 10);
            a.ytuc = q(a.ytuc, 10);
            const QString key = QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8 %9").arg(ch).arg(a.wght).arg(a.wdth).arg(a.slnt).arg(a.grad).arg(a.xtra).arg(a.yopq).arg(a.ytas).arg(a.ytuc);
            auto it = glyphs.constFind(key);
            if (it != glyphs.constEnd()) return *it;
            if (glyphs.size() > 6000) glyphs.clear();
            const QFont f = font(a);
            QPainterPath path;
            path.setFillRule(Qt::WindingFill); // Roboto Flex's outlines overlap
            const double adv = QFontMetricsF(f).horizontalAdvance(ch);
            path.addText(-adv / 2, 0, f, QString(ch));
            return *glyphs.insert(key, path);
        }

        double advance(QChar ch, const Axes& a) const {
            return QFontMetricsF(font(a)).horizontalAdvance(ch);
        }

        double headAt(double t) const {
            const double u = c01((t - launch) / (headArrive - launch));
            return kD + (headEnd - kD) * (0.75 * easeInOut(u) + 0.25 * u);
        }
        double flowAt(double t, double width) const {
            const double u = c01((t - headArrive) / (flowEnd - headArrive));
            return (path.length() - kD + width) * u * u * u;
        }
};

OnboardingIntro::OnboardingIntro(QWidget* parent) :
    QWidget(parent) {
    d = new OnboardingIntroPrivate();
    d->scheme = M3::currentScheme();
    this->setAttribute(Qt::WA_OpaquePaintEvent, false);
    d->frame.setInterval(16);
    d->frame.setTimerType(Qt::PreciseTimer);
    connect(&d->frame, &QTimer::timeout, this, &OnboardingIntro::tick);
    qApp->installEventFilter(this); // a key skips
}

OnboardingIntro::~OnboardingIntro() {
    qApp->removeEventFilter(this);
    delete d;
}

void OnboardingIntro::setPlacement(QPointF baselineLeft, double px) {
    d->left = baselineLeft.x();
    d->baseY = baselineLeft.y();
    d->px = px;
    if (!this->size().isEmpty()) this->resizeEvent(nullptr);
}

void OnboardingIntro::start(int delayMs) {
    d->started = true;
    d->offset = -delayMs;
    d->clock.start();
    d->frame.start();
}

void OnboardingIntro::setStays(bool stays) {
    d->stays = stays;
}

void OnboardingIntro::showResting() {
    d->started = true;
    d->done = true;
    this->update();
}

void OnboardingIntro::replay() {
    d->done = false;
    d->scheme = M3::currentScheme();
    start(0);
}

void OnboardingIntro::skip() {
    // Straight to the word at rest: the greeting takes over
    const double t = d->now();
    if (t >= endAt || !d->started) return;
    d->offset += qint64((endAt - t) * 1000);
}

void OnboardingIntro::mousePressEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    if (d->stays && d->done) {
        // At rest: once more
        replay();
        return;
    }
    skip();
}

bool OnboardingIntro::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched)
    // Staying (About): it plays alongside the page, keys are the page's
    if (event->type() == QEvent::KeyPress && !d->stays && this->isVisible() && d->started && !d->done) {
        skip();
        return true;
    }
    return false;
}

void OnboardingIntro::resizeEvent(QResizeEvent* event) {
    if (event) QWidget::resizeEvent(event);
    const double w = this->width();
    d->unit = d->px / 104.0;
    const double u = d->unit;
    d->glyphs.clear();

    const Axes rest;
    d->restD.clear();
    double x = 0;
    for (QChar ch : std::as_const(d->word)) {
        const double adv = d->advance(ch, rest);
        d->restD.append(x + adv / 2);
        x += adv;
    }
    d->kD = d->restD.last();
    d->kAdv = d->advance(QLatin1Char('k'), rest);
    // From the k: on a little, then a big loop down under the word (there's
    // no room above the greeting), a dip, and a swoop off the right edge
    const double kx = d->left + d->kD, by = d->baseY;
    auto at = [&](double dx, double dy) { return QPointF(kx + dx * u, by + dy * u); };
    d->path.build({QPointF(d->left - 100 * u, by), QPointF(d->left, by), QPointF(kx, by), at(90, 10), at(190, 110), at(170, 260),
        at(20, 340), at(-150, 280), at(-170, 140), at(-30, 90), at(120, 170), at(210, 320), at(260, 400), at(330, 330), at(380, 190),
        QPointF(w + 120 * u, by + 120 * u), QPointF(w + 500 * u, by + 100 * u), QPointF(w + 900 * u, by + 90 * u)});
    d->headEnd = d->kD;
    for (double dist = d->kD; dist < d->path.length(); dist += 2) {
        if (d->path.at(dist).x() > w - 60 * u) {
            d->headEnd = dist;
            break;
        }
    }
}

void OnboardingIntro::tick() {
    const double t = d->now();
    if (!d->done && t >= endAt) {
        d->done = true;
        d->frame.stop();
        if (d->stays) {
            this->update();
            emit finished();
            return;
        }
        this->hide();
        emit finished();
        this->deleteLater();
        return;
    }
    this->update();
}

namespace {
    // THEDESK_INTRO_STATS=1: how long frames take to draw
    struct PaintStats {
            QElapsedTimer timer;
            ~PaintStats() {
                static double total = 0;
                static int count = 0;
                static QElapsedTimer since;
                if (!since.isValid()) since.start();
                total += timer.nsecsElapsed() / 1e6;
                count++;
                if (since.elapsed() > 1000) {
                    qInfo("onboarding intro: %.1f ms per frame over %d frames", total / count, count);
                    total = 0;
                    count = 0;
                    since.restart();
                }
            }
    };
} // namespace

void OnboardingIntro::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    std::optional<PaintStats> stats;
    if (qEnvironmentVariableIsSet("THEDESK_INTRO_STATS")) {
        stats.emplace();
        stats->timer.start();
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const M3::Scheme& s = d->scheme;
    const double t = d->now();
    const double u = d->unit;
    const double px = d->px;
    const QString& word = d->word;

    if (d->done && d->stays) {
        // At rest, as it landed
        const Axes rest;
        for (int i = 0; i < word.size(); i++) {
            QTransform tr;
            tr.translate(d->left + d->restD[i], d->baseY);
            p.fillPath(tr.map(d->glyph(word.at(i), rest)), s.primary);
        }
        return;
    }
    if (t < 0 || d->done) return;

    auto draw = [&](QChar ch, double dist, const Axes& a, QColor colour, double extraRot = 0, double sx = 1, double sy = 1) {
        double ang;
        const QPointF pos = d->path.at(dist, &ang);
        QTransform tr;
        tr.translate(pos.x(), pos.y());
        tr.rotate(ang + extraRot); // turning with the path
        tr.scale(sx, sy);
        p.fillPath(tr.map(d->glyph(ch, a)), colour);
    };

    if (t < fallStart) {
        const double head = d->headAt(t), flow = d->flowAt(t, this->width());
        const double speed = (d->headAt(t + 1 / 60.0) - head + d->flowAt(t + 1 / 60.0, this->width()) - flow) * 60;
        const double v = c01(speed / (1400 * u));

        // ---- The word: written in, then pulled along the path
        double wx = 0;
        for (int i = 0; i < word.size(); i++) {
            const double k = qMin(spring(t - 0.08 * i, 0.5, 240), 1.15);
            Axes a;
            a.wght = 100 + 600 * c01(k);
            a.wdth = 25 + 100 * k;
            a.ytas = 650 + 100 * c01(k) + 480 * std::max(0.0, k - 1);
            double dist = d->restD[i];
            if (t < writeEnd + 0.3) {
                const double adv = d->advance(word.at(i), a);
                dist = wx + adv / 2;
                wx += adv;
            }
            double rot = 0, sy = 1;
            const bool isK = i == word.size() - 1;
            if (isK && t >= crouch && t < launch + 0.4) {
                // The crouch: short, narrow, heavy, leaning back; then the pop
                const double c = t < launch ? smooth((t - crouch) / (launch - crouch)) : 1 - spring(t - launch, 0.35, 420);
                a.ytas = 750 - 100 * c;
                a.wdth = 125 - 50 * c;
                a.wght = 700 + 250 * c;
                a.slnt = -10 * c;
                sy = 1 - 0.12 * c;
                rot = -6 * c;
            }
            if (!isK) {
                // The word flinches as the stream starts, back from the k
                // (grade and ascenders: heavier and taller, never wider)
                const double w = t - launch - 0.05 * (word.size() - 1 - i);
                if (w > 0 && w < 0.6) {
                    const double e = std::sin(w / 0.6 * M_PI);
                    a.grad += 150 * e;
                    a.ytas += 90 * e;
                    a.yopq += 30 * e;
                }
            }
            if (k <= 0.005) continue;
            draw(word.at(i), dist + flow, a, s.primary, rot, 1, sy);
        }

        // ---- The stream: k's laid back from the head to the word's own k,
        // so it never leaves the word
        if (t >= launch) {
            const double sp = d->kAdv * 1.02;
            const int count = int((head - d->kD) / sp) + 1;
            for (int j = 0; j <= count; j++) {
                const double dist = head - j * sp;
                const double emerge = c01((dist - d->kD) / sp); // coming out of the word's k
                if (emerge <= 0) continue;
                double ang, climb, bend;
                d->path.at(dist + flow, &ang, &climb, &bend);
                // (snapped to a few steps: the outlines come from the cache,
                // and on a moving letter the steps don't show)
                const double step = 2 * M_PI / 12;
                const double phase = std::round((j * 0.55 - t * 7) / step) * step;
                const double vq = std::round(v * 4) / 4;
                climb = std::round(climb * 2) / 2;
                const double lean = std::round(c01(bend * 60 / u) * 3) / 3;
                Axes a;
                a.wght = 420 + 380 * vq + 160 * std::sin(phase);
                a.wdth = 70 + 55 * vq + 18 * std::sin(phase + 1.2);
                a.ytas = 750 + 104 * c01(climb) - 100 * c01(-climb); // reaching up, ducking down
                a.slnt = -10 * lean;                                  // leaning into the turns
                a.xtra = 468 + 110 * std::sin(phase * 0.7);
                a.yopq = 79 + 45 * std::sin(phase + 2.1);
                a.grad = 120 * vq;
                const double scale = 0.35 + 0.65 * smooth(emerge);
                const double along = count > 0 ? double(j) / count : 0;
                const QColor colour = j == 0 ? s.tertiary : mix(s.primary, s.tertiary, 1 - along);
                const double headGrow = j == 0 ? 1.1 + 0.15 * v : 1.0;
                draw(QLatin1Char('k'), dist + flow, a, colour, 0, scale * headGrow * (1 + (j == 0 ? 0.1 * v : 0)), scale * headGrow);
            }
        }
        return;
    }

    // ---- The fall: tumbling in from the top, a squash, one breath
    const double g = 3000 * u;
    for (int i = 0; i < word.size(); i++) {
        const bool isK = i == word.size() - 1;
        const double start = fallStart + 0.08 * i + (isK ? 0.12 : 0);
        if (t < start) continue;
        const double top = -d->baseY - 120 * u; // from above the card
        const double landT = start + std::sqrt(2 * (d->baseY - top) / g);
        QPointF pos(d->left + d->restD[i], d->baseY);
        double rot = 0, sx = 1, sy = 1;
        Axes a;
        if (t < landT) {
            const double dt = t - start;
            pos.setY(top + 0.5 * g * dt * dt);
            const double v = c01(g * dt / (2400 * u));
            a.wdth = 125 - 50 * v;
            a.wght = 700 - 300 * v;
            a.ytas = 750 + 104 * v;
            a.yopq = 79 - 40 * v;
            sy = 1 + 0.1 * v;
            rot = (i % 2 ? 1 : -1) * 120 * (1 - c01(dt / (landT - start))); // tumbling, righting itself
        } else {
            const double q = 1 - spring(t - landT, isK ? 0.28 : 0.38, isK ? 420 : 520);
            sy = 1 - 0.3 * q;
            sx = 1 + 0.08 * q;
            a.wdth = 125 + 12 * q;
            a.wght = 700 + 120 * q;
            a.grad = 150 * c01(q) - 100 * c01(-q);
            a.ytas = 750 - 100 * q;
            a.yopq = 79 + 40 * q;
            if (isK) rot = 14 * std::exp(-4 * (t - landT)) * std::sin(18 * (t - landT)); // the k's wobble
            // The settle breath: a wave through the word (grade, ascenders,
            // stroke contrast, capitals; never wider)
            const double b = (t - (fallStart + 1.35 + 0.06 * i)) / 0.6;
            if (b > 0 && b < 1) {
                const double e = std::sin(M_PI * b);
                a.grad += 150 * e;
                a.ytas += 100 * e;
                a.yopq += 35 * e;
                a.ytuc += 40 * e;
            }
        }
        QTransform tr;
        tr.translate(pos.x(), pos.y());
        tr.rotate(rot);
        tr.scale(sx, sy);
        p.fillPath(tr.map(d->glyph(word.at(i), a)), s.primary);
    }
}
