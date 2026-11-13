/****************************************
 *
 *   theDesk - an incoming call on a linked iPhone
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "callcard.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QtMath>
#include <compositor.h>
#include <desksound.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>

namespace {
    constexpr double Width = 420;
    constexpr double Height = 336;
    constexpr double Margin = 24;   // room for the shadow
    constexpr double Top = 132;     // under the bar
    constexpr double RingEvery = 1.6; // seconds between beats

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

    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    QFont symbol(double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qMax(1, qRound(px)));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        return font;
    }

    QPainterPath wavyCircle(QPointF centre, double radius, double amplitude, double wavelength, double phase) {
        QPainterPath path;
        const int steps = qMax(64, int(2 * M_PI * radius / 2));
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
} // namespace

struct CallCardPrivate {
        QString caller, phone;
        std::function<void(bool)> answer;

        enum State {
            Ringing,
            Answered,
            Declined,
            Ended
        } state = Ringing;

        QTimer* ticker;
        QElapsedTimer clock;
        double time = 0;
        double nextRing = 0.5;
        QList<double> rings; // when each beat was
        double leaveAt = -1;

        Spring drop, beat, morph, weight, press, answered, leave;
        int hovered = 0; // 1 decline, 2 answer
        QPointer<DeskSound> ringtone;
        int pressed = 0;
        std::unique_ptr<m3shapes::Morph> picture, tick;
};

bool CallCard::available() {
    return Compositor::isActive();
}

CallCard::CallCard(QString caller, QString phone, std::function<void(bool)> answer) :
    QWidget(nullptr) {
    d = new CallCardPrivate();
    d->caller = caller;
    d->phone = phone;
    d->answer = std::move(answer);
    d->picture = std::make_unique<m3shapes::Morph>(MaterialShapes::cookie9Sided(), MaterialShapes::sunny());
    d->tick = std::make_unique<m3shapes::Morph>(MaterialShapes::cookie9Sided(), MaterialShapes::circle());

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMouseTracking(true);
    this->setWindowTitle(tr("Incoming call"));
    this->setAccessibleName(tr("Incoming call from %1").arg(caller));

    // Top centre of the main screen, under the bar
    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect area = screen ? screen->geometry() : QRect(0, 0, 1366, 768);
    this->setGeometry(area.left() + int((area.width() - Width) / 2 - Margin), area.top() + int(Top - Margin), int(Width + 2 * Margin), int(Height + 2 * Margin + 40));

    d->drop.spec = M3Motion::spring(M3Motion::SpatialDefault);
    d->drop.target = 1;
    d->beat.spec = {0.5, 520}; // a quick knock on each ring
    d->morph.spec = M3Motion::spring(M3Motion::SpatialSlow);
    d->weight.x = d->weight.target = 560;
    d->weight.spec = M3Motion::spring(M3Motion::SpatialDefault);
    d->press.spec = M3Motion::spring(M3Motion::SpatialFast);
    d->answered.spec = M3Motion::spring(M3Motion::EffectsDefault);
    d->leave.spec = M3Motion::spring(M3Motion::SpatialDefault);

    d->ticker = new QTimer(this);
    d->ticker->setInterval(16);
    connect(d->ticker, &QTimer::timeout, this, &CallCard::tick);
    d->clock.start();
    d->ticker->start();
    this->show();

    d->ringtone = DeskSound::play(QStringLiteral("phone-incoming-call"), DeskSound::Ringtone);
}

CallCard::~CallCard() {
    if (d->ringtone) d->ringtone->stop();
    delete d;
}

void CallCard::end() {
    // Answered or declined on the phone, or the caller hung up
    if (d->state != CallCardPrivate::Ringing) return;
    d->state = CallCardPrivate::Ended;
    d->leaveAt = d->time;
    if (d->ringtone) d->ringtone->stop();
}

void CallCard::choose(bool answer) {
    if (d->state != CallCardPrivate::Ringing) return;
    d->state = answer ? CallCardPrivate::Answered : CallCardPrivate::Declined;
    d->answered.target = 1;
    if (d->ringtone) d->ringtone->stop();
    d->leaveAt = d->time + (answer ? 1.8 : 0.9);
    if (d->answer) d->answer(answer);
}

void CallCard::tick() {
    const double dt = qMin(0.05, d->clock.restart() / 1000.0);
    d->time += dt;

    // A beat on each ring: the picture knocks and morphs, the name thickens
    if (d->state == CallCardPrivate::Ringing && d->time >= d->nextRing) {
        d->nextRing += RingEvery;
        d->rings.append(d->time);
        d->beat.v += 9;
        if (M3Motion::expressive()) {
            d->morph.target = d->morph.target > 0.5 ? 0 : 1;
            d->weight.target = d->weight.target > 600 ? 560 : 720;
        }
    }
    d->rings.removeIf([this](double at) {
        return d->time - at > 2;
    });

    if (d->leaveAt >= 0 && d->time >= d->leaveAt) d->leave.target = 1;
    d->press.target = d->pressed ? 1 : 0;
    for (Spring* spring : {&d->drop, &d->beat, &d->morph, &d->weight, &d->press, &d->answered, &d->leave}) spring->step(dt);
    this->update();

    if (d->leave.x > 0.98) {
        d->ticker->stop();
        this->close();
    }
}

QRectF CallCard::card() const {
    // Slides down from under the bar, and back up when done
    const double slide = (1 - d->drop.x) * -(Height + 160) + d->leave.x * -(Height + 180);
    return QRectF(Margin, Margin + slide, Width, Height);
}

QRectF CallCard::declineRect() const {
    const QRectF c = card();
    return QRectF(c.left() + 24, c.bottom() - 24 - 56, Width / 2 - 30, 56);
}

QRectF CallCard::answerRect() const {
    const QRectF c = card();
    return QRectF(c.left() + Width / 2 + 6, c.bottom() - 24 - 56, Width / 2 - 30, 56);
}

void CallCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    // Answer is green: a scheme seeded green sits with the rest of the colours
    static const M3::Scheme green = M3::Scheme::generate(QColor(0x3f, 0xa3, 0x4d), true);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRectF c = card();
    const double t = d->time;
    const double done = qBound(0.0, d->answered.x, 1.0);
    const bool answered = d->state == CallCardPrivate::Answered;

    // Shadow, then the card
    p.setPen(Qt::NoPen);
    for (int i = 6; i >= 1; i--) {
        p.setBrush(QColor(0, 0, 0, 10));
        p.drawRoundedRect(c.adjusted(-i * 2, -i * 2 + 6, i * 2, i * 2 + 6), 28 + i * 2, 28 + i * 2);
    }
    p.setBrush(s.surfaceContainerHigh);
    QPainterPath shape;
    shape.addRoundedRect(c, 28, 28);
    p.drawPath(shape);

    const QPointF picture(c.center().x(), c.top() + 92);

    // Wavy rings spread from the picture on each ring
    p.save();
    p.setClipPath(shape);
    for (double at : std::as_const(d->rings)) {
        for (int wave = 0; wave < 2; wave++) {
            const double k = (t - at - wave * 0.18) / 1.4;
            if (k <= 0 || k >= 1) continue;
            const double radius = 52 + (1 - std::pow(1 - k, 2)) * 170;
            QColor colour = s.primary;
            colour.setAlphaF(float(0.55 * (1 - k) * (1 - k) * (wave ? 0.6 : 1) * (1 - done)));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(colour, 2.5));
            p.drawPath(wavyCircle(picture, radius, 3, 30, -t * 4));
        }
    }
    p.restore();

    // The caller: their initial in a shape that beats; answered, a green phone
    const double size = 104 * (1 + 0.08 * d->beat.x);
    QTransform turn;
    turn.translate(picture.x(), picture.y());
    turn.rotate(M3Motion::expressive() ? t * 10 : 0);
    p.setPen(Qt::NoPen);
    if (!answered || done < 0.99) {
        const double fade = answered ? 1 - done : 1;
        QColor fill = s.primaryContainer, ink = s.onPrimaryContainer;
        fill.setAlphaF(float(fade));
        ink.setAlphaF(float(fade));
        p.setBrush(fill);
        p.drawPath(turn.map(m3shapes::toPath(*d->picture, float(qBound(0.0, d->morph.x, 1.0)), QRectF(-size / 2, -size / 2, size, size))));
        const QString initial = d->caller.trimmed().left(1).toUpper();
        const bool letter = !initial.isEmpty() && initial.at(0).isLetter();
        p.setPen(ink);
        p.setFont(letter ? flex(48, 760, 110) : symbol(48));
        p.drawText(QRectF(picture.x() - 60, picture.y() - 60, 120, 120), Qt::AlignCenter, letter ? initial : QStringLiteral("person"));
    }
    if (answered && done > 0.01) {
        QColor fill = green.primaryContainer, ink = green.onPrimaryContainer;
        fill.setAlphaF(float(done));
        ink.setAlphaF(float(done));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(turn.map(m3shapes::toPath(*d->tick, float(done), QRectF(-52, -52, 104, 104))));
        p.setPen(ink);
        p.setFont(symbol(52));
        p.drawText(QRectF(picture.x() - 60, picture.y() - 60, 120, 120), Qt::AlignCenter, QStringLiteral("call"));
    }

    // Name and what's happening
    p.setPen(s.onSurface);
    p.setFont(flex(34, d->weight.x, 105));
    p.drawText(QRectF(c.left() + 16, c.top() + 152, Width - 32, 44), Qt::AlignCenter, p.fontMetrics().elidedText(d->caller, Qt::ElideRight, int(Width - 32)));
    QString line = d->phone.isEmpty() ? tr("Incoming call") : tr("%1 · Incoming call").arg(d->phone);
    if (d->state == CallCardPrivate::Answered) line = d->phone.isEmpty() ? tr("Answered") : tr("Answered on %1").arg(d->phone);
    if (d->state == CallCardPrivate::Declined) line = tr("Declined");
    p.setPen(s.onSurfaceVariant);
    p.setFont(flex(15, 450));
    p.drawText(QRectF(c.left(), c.top() + 196, Width, 22), Qt::AlignCenter, line);

    // Decline, unmistakably red; Answer, green. Pressing tightens the corners
    if (done < 0.99) {
        p.setOpacity(1 - done);
        auto pill = [&](QRectF r, int which, QColor fill, QColor ink, const QString& icon, const QString& text) {
            const double pressed = d->pressed == which ? qBound(0.0, d->press.x, 1.0) : 0;
            r.adjust(-4 * pressed, 0, 4 * pressed, 0);
            if (d->hovered == which) fill = M3::layer(fill, ink, 0.08);
            if (d->pressed == which) fill = M3::layer(fill, ink, 0.10);
            QPainterPath path;
            const double radius = 28 - 16 * pressed;
            path.addRoundedRect(r, radius, radius);
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPath(path);
            p.setPen(ink);
            p.setFont(symbol(24));
            p.drawText(QRectF(r.left() + 22, r.top(), 30, r.height()), Qt::AlignCenter, icon);
            p.setFont(flex(16, 600));
            p.drawText(r.adjusted(40, 0, 0, 0), Qt::AlignCenter, text);
        };
        pill(declineRect(), 1, s.error, s.onError, QStringLiteral("call_end"), tr("Decline"));
        pill(answerRect(), 2, green.primary, green.onPrimary, QStringLiteral("call"), tr("Answer"));
        p.setOpacity(1);
    }
    if (answered && done > 0.01) {
        // The call itself stays on the phone
        p.setOpacity(done);
        p.setPen(s.onSurfaceVariant);
        p.setFont(flex(14, 450));
        p.drawText(QRectF(c.left(), c.bottom() - 24 - 56, Width, 56), Qt::AlignCenter, d->phone.isEmpty() ? tr("Talk on your iPhone") : tr("Talk on %1").arg(d->phone));
        p.setOpacity(1);
    }
}

void CallCard::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    const int hovered = declineRect().contains(pos) ? 1 : answerRect().contains(pos) ? 2 : 0;
    if (hovered != d->hovered) {
        d->hovered = hovered;
        this->setCursor(hovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void CallCard::mousePressEvent(QMouseEvent* event) {
    d->pressed = declineRect().contains(event->position()) ? 1 : answerRect().contains(event->position()) ? 2 : 0;
}

void CallCard::mouseReleaseEvent(QMouseEvent* event) {
    const int pressed = d->pressed;
    d->pressed = 0;
    if (pressed == 1 && declineRect().contains(event->position())) choose(false);
    if (pressed == 2 && answerRect().contains(event->position())) choose(true);
}

void CallCard::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = 0;
}
