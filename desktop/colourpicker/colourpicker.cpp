#include "colourpicker.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <cmath>
#include <keygrab.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>

namespace {
    constexpr double Radius = 60;    // the drop's round part
    constexpr double Swatch = 124;
    constexpr double FanDistance = 118;
    constexpr double Tone = 46;

    struct Spring {
            double value = 0, velocity = 0, target = 0;
            void step(M3Motion::Token token, double dt) {
                const auto s = M3Motion::stepSpring(value, velocity, target, M3Motion::spring(token), dt);
                value = s.value;
                velocity = s.velocity;
            }
    };

    QFont flex(double px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }

    QFont mono(double px) {
        QFont font(QStringLiteral("Google Sans Code"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), 600);
        return font;
    }

    QColor inkOn(QColor colour) {
        return qGray(colour.rgb()) > 140 ? QColor(24, 24, 24) : QColor(Qt::white);
    }

    QString hex(QColor colour) {
        return colour.name().toUpper();
    }

    // The drop: round at the back, its tip at the hotspot, pointing along +x
    // before it's turned
    QPainterPath dropPath(double r) {
        QPainterPath path;
        path.moveTo(0, 0);
        path.cubicTo(QPointF(r * 0.35, -r * 0.35), QPointF(r * 0.25, -r), QPointF(r * 1.25, -r));
        path.arcTo(QRectF(r * 0.25, -r, 2 * r, 2 * r), 90, -180);
        path.cubicTo(QPointF(r * 0.25, r), QPointF(r * 0.35, r * 0.35), QPointF(0, 0));
        return path;
    }

    double shortestTurn(double from, double to) {
        double turn = std::fmod(to - from + 540, 360) - 180;
        return turn;
    }
} // namespace

struct ColourPicker::Private {
        QImage shot;
        QRect area;
        QPointF at, last, velocity;
        double angle = 45; // the drop trails its motion; at rest it hangs down-right
        int zoom = 11;
        enum { Picking, Dropped } phase = Picking;

        Spring appear, stretch, fall, open, fan, leave;
        QTimer tick;
        QElapsedTimer clock, idle;

        QColor picked, copied;
        QPointF from, landing;
        QColor tones[5];
        QRectF toneRects[5];
        int hoveredTone = -1;
        bool leaving = false;
};

void ColourPicker::setUp() {
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::ShiftModifier | Qt::Key_P), QStringLiteral("colourPicker")), &KeyGrab::activated, [] {
        pick();
    });
}

void ColourPicker::pick() {
    static ColourPicker* open = nullptr;
    if (open) return;
    open = new ColourPicker();
    connect(open, &QObject::destroyed, [] {
        open = nullptr;
    });
}

ColourPicker::ColourPicker() :
    QWidget(nullptr) {
    d = new Private();
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::Tool);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMouseTracking(true);
    this->setCursor(Qt::BlankCursor);

    // The screen, frozen: what's under the drop doesn't move while picking
    for (QScreen* screen : qApp->screens()) d->area = d->area.united(screen->geometry());
    d->shot = qApp->primaryScreen()->grabWindow(0, d->area.x(), d->area.y(), d->area.width(), d->area.height()).toImage().convertToFormat(QImage::Format_RGB32);
    d->shot.setDevicePixelRatio(1);

    d->at = d->last = QCursor::pos() - d->area.topLeft();
    d->appear.target = 1;
    d->tick.setInterval(16);
    connect(&d->tick, &QTimer::timeout, this, &ColourPicker::frame);
    d->clock.start();
    d->idle.start();

    this->setGeometry(d->area);
    this->show();
    this->activateWindow();
    this->grabKeyboard();
    d->tick.start();
}

ColourPicker::~ColourPicker() {
    delete d;
}

QColor ColourPicker::colourAt(QPoint point) const {
    return d->shot.pixelColor(qBound(0, point.x(), d->shot.width() - 1), qBound(0, point.y(), d->shot.height() - 1));
}

void ColourPicker::frame() {
    const double dt = qMin(0.05, d->clock.restart() / 1000.0);
    // How fast the pointer goes, smoothed, turns and stretches the drop
    const QPointF raw = (d->at - d->last) / qMax(dt, 0.001);
    d->last = d->at;
    d->velocity = d->velocity * 0.7 + raw * 0.3;
    const double speed = std::hypot(d->velocity.x(), d->velocity.y());
    if (d->phase == Private::Picking) {
        const double target = speed > 60 ? std::atan2(d->velocity.y(), d->velocity.x()) * 180 / M_PI + 180 : 45;
        d->angle += shortestTurn(d->angle, target) * qMin(1.0, dt * 12);
        d->stretch.target = M3Motion::expressive() ? qMin(1.0, speed / 1400) : 0;
    }

    d->appear.step(M3Motion::SpatialFast, dt);
    d->stretch.step(M3Motion::SpatialFast, dt);
    d->fall.step(M3Motion::SpatialDefault, dt);
    if (d->phase == Private::Dropped && d->fall.value > 0.7) d->open.target = 1;
    d->open.step(M3Motion::SpatialDefault, dt);
    if (d->open.value > 0.6) d->fan.target = 1;
    d->fan.step(M3Motion::SpatialDefault, dt);
    d->leave.step(M3Motion::EffectsFast, dt);

    if (d->leaving && d->leave.value > 0.97) {
        this->close();
        return;
    }
    // Left alone after a pick, it goes away by itself
    if (d->phase == Private::Dropped && !d->leaving && d->idle.elapsed() > 6000) finish();
    this->update();
}

void ColourPicker::drop() {
    if (d->phase != Private::Picking) return;
    d->phase = Private::Dropped;
    d->picked = colourAt(d->at.toPoint());
    d->from = d->at;
    // Where it lands: below the pointer, kept inside the screen with room for the fan
    const double margin = FanDistance + Tone;
    d->landing = QPointF(qBound(margin, d->at.x(), this->width() - margin), qBound(margin, d->at.y() + 150, this->height() - margin - 60));
    d->fall.target = M3Motion::expressive() ? 1 : 1;
    if (!M3Motion::expressive()) d->fall.value = 1;

    // The family: the colour's own hue and chroma through the tones, light to dark
    const M3::Scheme light = M3::Scheme::generate(d->picked, false, M3::Variant::Fidelity);
    const M3::Scheme dark = M3::Scheme::generate(d->picked, true, M3::Variant::Fidelity);
    const QColor tones[] = {light.primaryContainer, dark.primary, light.primary, dark.primaryContainer, light.onPrimaryContainer};
    for (int i = 0; i < 5; i++) d->tones[i] = tones[i];
    this->setCursor(Qt::ArrowCursor);
    copy(d->picked);
}

void ColourPicker::copy(QColor colour) {
    d->copied = colour;
    d->idle.restart();
    QGuiApplication::clipboard()->setText(hex(colour));
}

void ColourPicker::finish() {
    if (d->leaving) return;
    d->leaving = true;
    d->leave.target = 1;
    this->releaseKeyboard();
    if (!M3Motion::expressive()) this->close();
}

void ColourPicker::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.drawImage(0, 0, d->shot);
    const double fade = 1 - qBound(0.0, d->leave.value, 1.0);
    const double open = qMax(0.0, d->open.value);

    if (d->phase == Private::Dropped) {
        // The rest of the screen steps back while the swatch is out
        QColor scrim(0, 0, 0);
        scrim.setAlphaF(float(0.28 * qBound(0.0, open, 1.0) * fade));
        p.fillRect(this->rect(), scrim);
    }
    p.setOpacity(fade);

    // The drop, until it has fallen into the swatch
    const double fall = qBound(0.0, d->fall.value, 1.0);
    if (fall < 0.98) {
        const QPointF at = d->phase == Private::Picking ? d->at : d->from + (d->landing - d->from) * fall;
        const QColor under = d->phase == Private::Picking ? colourAt(d->at.toPoint()) : d->picked;
        const double s = 1 + 0.35 * qMax(0.0, d->stretch.value);
        const double scale = qMax(0.0, d->appear.value) * (1 - 0.75 * fall);
        QTransform shape;
        shape.rotate(d->angle);
        shape.scale(s, 1 / std::sqrt(s));
        const QPainterPath path = shape.map(dropPath(Radius));

        p.save();
        p.translate(at);
        p.scale(scale, scale);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 70));
        p.drawPath(path.translated(0, 5));
        // Magnified pixels inside, the one under the tip framed in the middle
        p.save();
        p.setClipPath(path);
        const QPointF centre = shape.map(QPointF(Radius * 1.25, 0));
        const int n = int(2 * Radius * s / d->zoom) + 4;
        const QPoint source = d->phase == Private::Picking ? d->at.toPoint() : d->from.toPoint();
        const QImage part = d->shot.copy(source.x() - n / 2, source.y() - n / 2, n, n).scaled(n * d->zoom, n * d->zoom);
        p.drawImage(centre - QPointF((n / 2 + 0.5) * d->zoom, (n / 2 + 0.5) * d->zoom), part);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 230), 2));
        p.drawRect(QRectF(centre.x() - d->zoom / 2.0, centre.y() - d->zoom / 2.0, d->zoom, d->zoom));
        p.restore();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(under, 8));
        p.drawPath(path);
        p.setPen(QPen(QColor(255, 255, 255, 220), 2));
        p.drawPath(path);
        p.restore();

        // While picking, the hex rides under the drop
        if (d->phase == Private::Picking) {
            p.setFont(mono(14));
            const QString text = hex(under);
            const double w = QFontMetrics(p.font()).horizontalAdvance(text) + 24;
            const QPointF anchor = at + QTransform().rotate(d->angle).map(QPointF(Radius * 1.25, 0)) * scale + QPointF(0, Radius * scale + 22);
            const QRectF pill(anchor.x() - w / 2, anchor.y() - 14, w, 28);
            p.setPen(Qt::NoPen);
            p.setBrush(under);
            p.drawRoundedRect(pill, 14, 14);
            p.setPen(inkOn(under));
            p.drawText(pill, Qt::AlignCenter, text);
        }
    }

    if (d->phase != Private::Dropped || open <= 0.001) return;

    // The family fans out from behind the swatch
    const m3shapes::RoundedPolygon* shapes[] = {&MaterialShapes::sunny(), &MaterialShapes::clover4Leaf(), &MaterialShapes::cookie6Sided(), &MaterialShapes::gem(), &MaterialShapes::pentagon()};
    const double fan = qMax(0.0, d->fan.value);
    for (int i = 0; i < 5; i++) {
        const double a = (30 + i * 30) * M_PI / 180;
        const QPointF pos = d->landing + QPointF(-std::cos(a) * FanDistance * fan, -std::sin(a) * FanDistance * fan * 0.95 + 20 * fan);
        const double size = Tone * qMin(1.0, fan) * (i == d->hoveredTone ? 1.18 : 1);
        d->toneRects[i] = QRectF(pos.x() - size / 2, pos.y() - size / 2, size, size);
        if (size < 1) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawPath(m3shapes::toPath(*shapes[i], d->toneRects[i].translated(0, 3)));
        p.setBrush(d->tones[i]);
        p.drawPath(m3shapes::toPath(*shapes[i], d->toneRects[i]));
    }

    // The swatch the drop fell into
    const double size = Swatch * open;
    const QRectF swatch(d->landing.x() - size / 2, d->landing.y() - size / 2, size, size);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 80));
    p.drawPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), swatch.translated(0, 5)));
    p.setBrush(d->picked);
    p.drawPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), swatch));
    if (open > 0.7) {
        p.setPen(inkOn(d->picked));
        p.setFont(mono(17));
        p.drawText(swatch, Qt::AlignCenter, hex(d->picked));
    }

    // What was copied, and what else can be
    const double pill = qBound(0.0, (open - 0.8) * 5, 1.0);
    if (pill > 0) {
        const M3::Scheme s = M3::currentScheme();
        p.setOpacity(fade * pill);
        p.setFont(flex(14, 560));
        const QString text = tr("%1 copied").arg(hex(d->copied));
        const QString hint = d->hoveredTone >= 0 ? tr("Click to copy %1").arg(hex(d->tones[d->hoveredTone])) : tr("Click a tone to copy it instead");
        const double w = qMax(QFontMetrics(p.font()).horizontalAdvance(text) + 56.0, QFontMetrics(flex(12, 450)).horizontalAdvance(hint) + 32.0);
        const QRectF card(d->landing.x() - w / 2, d->landing.y() + Swatch / 2 + 16 + (1 - pill) * 10, w, 56);
        p.setPen(Qt::NoPen);
        p.setBrush(s.surfaceContainerHigh);
        p.drawRoundedRect(card, 20, 20);
        QFont symbol(QStringLiteral("Material Symbols Rounded"));
        symbol.setPixelSize(18);
        symbol.setVariableAxis(QFont::Tag("FILL"), 1);
        p.setFont(symbol);
        p.setPen(s.primary);
        p.drawText(QRectF(card.left() + 14, card.top() + 8, 20, 22), Qt::AlignCenter, QStringLiteral("check"));
        p.setFont(flex(14, 560));
        p.setPen(s.onSurface);
        p.drawText(QRectF(card.left() + 40, card.top() + 8, w - 52, 22), Qt::AlignVCenter | Qt::AlignLeft, text);
        p.setFont(flex(12, 450));
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(card.left() + 16, card.top() + 30, w - 32, 18), Qt::AlignVCenter | Qt::AlignLeft, hint);
    }
}

void ColourPicker::mouseMoveEvent(QMouseEvent* event) {
    if (d->phase == Private::Picking) {
        d->at = event->position();
        return;
    }
    int hovered = -1;
    for (int i = 0; i < 5; i++) {
        if (d->toneRects[i].contains(event->position())) hovered = i;
    }
    if (hovered != d->hoveredTone) {
        d->hoveredTone = hovered;
        d->idle.restart();
        this->setCursor(hovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void ColourPicker::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        finish();
        return;
    }
    if (d->phase == Private::Picking) {
        d->at = event->position();
        drop();
        return;
    }
    for (int i = 0; i < 5; i++) {
        if (d->toneRects[i].contains(event->position())) {
            copy(d->tones[i]);
            return;
        }
    }
    const QRectF swatch(d->landing.x() - Swatch / 2, d->landing.y() - Swatch / 2, Swatch, Swatch);
    if (swatch.contains(event->position())) copy(d->picked);
    else finish();
}

void ColourPicker::wheelEvent(QWheelEvent* event) {
    if (d->phase != Private::Picking) return;
    d->zoom = qBound(5, d->zoom + (event->angleDelta().y() > 0 ? 2 : -2), 25);
}

void ColourPicker::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            finish();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (d->phase == Private::Picking) drop();
            else finish();
            return;
        default:
            break;
    }
    // Arrows move the drop a pixel at a time (Shift: ten)
    if (d->phase != Private::Picking) return;
    const int step = event->modifiers() & Qt::ShiftModifier ? 10 : 1;
    QPoint move;
    if (event->key() == Qt::Key_Left) move = QPoint(-step, 0);
    if (event->key() == Qt::Key_Right) move = QPoint(step, 0);
    if (event->key() == Qt::Key_Up) move = QPoint(0, -step);
    if (event->key() == Qt::Key_Down) move = QPoint(0, step);
    if (move.isNull()) return;
    QCursor::setPos(QCursor::pos() + move);
    d->at += move;
}
