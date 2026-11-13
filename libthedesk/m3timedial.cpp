#include "m3timedial.h"

#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <functional>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    // TimePickerTokens
    constexpr double BoxW = 96, BoxH = 80, Colon = 24, PeriodW = 52, PeriodGap = 12;
    constexpr double Dial = 256, Handle = 48, Outer = 104, Inner = 68, Gap = 24;
    constexpr double Top = 4; // room for the focus ring

    QFont flex(int px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(px);
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }
} // namespace

struct M3TimeDial::Private {
        QTime time = QTime(0, 0);
        bool minutes = false;
        bool twelve = false;
        bool dragging = false;
        bool keyboard = false; // the focus ring shows for keys, not clicks
        M3SpringAnimation* angle;
        M3SpringAnimation* radius;

        double width = 0;
        QRectF hourBox, minuteBox, am, pm;
        QPointF centre;
};

M3TimeDial::M3TimeDial(QWidget* parent) :
    QWidget(parent) {
    d = new Private();
    d->twelve = QLocale().timeFormat(QLocale::ShortFormat).contains(QLatin1Char('a'), Qt::CaseInsensitive);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setCursor(Qt::PointingHandCursor);
    d->angle = new M3SpringAnimation(this);
    d->angle->setToken(M3Motion::SpatialDefault);
    d->radius = new M3SpringAnimation(this);
    d->radius->setToken(M3Motion::SpatialDefault);
    connect(d->angle, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(d->radius, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    d->angle->snapTo(0.0);
    d->radius->snapTo(Outer);
}

M3TimeDial::~M3TimeDial() {
    delete d;
}

QTime M3TimeDial::time() const {
    return d->time;
}

void M3TimeDial::setTime(QTime time) {
    if (!time.isValid()) return;
    time = QTime(time.hour(), time.minute());
    const bool changed = time != d->time;
    d->time = time;
    aimHandle();
    if (changed) emit timeChanged(time);
}

QSize M3TimeDial::sizeHint() const {
    return QSize(328, int(Top + BoxH + Gap + Dial + 8));
}

void M3TimeDial::setMinutesMode(bool minutes) {
    d->minutes = minutes;
    aimHandle();
}

void M3TimeDial::aimHandle() {
    // where the handle should point for the current mode, the short way round
    double target;
    double radius = Outer;
    if (d->minutes) {
        target = d->time.minute() * 6.0;
    } else {
        target = (d->time.hour() % 12) * 30.0;
        if (!d->twelve && d->time.hour() >= 12) radius = Inner;
    }
    const double now = d->angle->currentValue().toDouble();
    double delta = std::fmod(target - now, 360.0);
    if (delta > 180) delta -= 360;
    if (delta < -180) delta += 360;
    if (d->dragging) {
        d->angle->snapTo(now + delta);
        d->radius->snapTo(radius);
    } else {
        d->angle->animateTo(now + delta);
        d->radius->animateTo(radius);
    }
    this->update();
}

void M3TimeDial::pick(QPointF at, bool settle) {
    const QPointF v = at - d->centre;
    const double distance = std::hypot(v.x(), v.y());
    double angle = std::atan2(v.x(), -v.y()) * 180 / M_PI;
    if (angle < 0) angle += 360;
    QTime time = d->time;
    if (d->minutes) {
        time = QTime(time.hour(), int(std::round(angle / 6)) % 60);
    } else {
        int hour = int(std::round(angle / 30)) % 12;
        if (d->twelve) {
            if (time.hour() >= 12) hour += 12;
        } else if (distance < (Outer + Inner) / 2) {
            hour += 12;
        }
        time = QTime(hour, time.minute());
    }
    setTime(time);
    if (settle && !d->minutes) setMinutesMode(true); // an hour picked: on to the minutes
}

void M3TimeDial::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // The header: hour : minute (and AM / PM)
    const double headerW = BoxW * 2 + Colon + (d->twelve ? PeriodGap + PeriodW : 0);
    const double left = (this->width() - headerW) / 2;
    d->hourBox = QRectF(left, Top, BoxW, BoxH);
    d->minuteBox = QRectF(left + BoxW + Colon, Top, BoxW, BoxH);
    auto box = [&](const QRectF& r, const QString& text, bool selected) {
        p.setPen(Qt::NoPen);
        p.setBrush(selected ? s.primaryContainer : s.surfaceContainerHighest);
        p.drawRoundedRect(r, 8, 8);
        p.setFont(flex(57, 400));
        p.setPen(selected ? s.onPrimaryContainer : s.onSurface);
        p.drawText(r, Qt::AlignCenter, text);
    };
    const int hour = d->twelve ? (d->time.hour() % 12 == 0 ? 12 : d->time.hour() % 12) : d->time.hour();
    box(d->hourBox, d->twelve ? QString::number(hour) : QStringLiteral("%1").arg(hour, 2, 10, QLatin1Char('0')), !d->minutes);
    box(d->minuteBox, QStringLiteral("%1").arg(d->time.minute(), 2, 10, QLatin1Char('0')), d->minutes);
    p.setFont(flex(57, 400));
    p.setPen(s.onSurface);
    p.drawText(QRectF(d->hourBox.right(), Top, Colon, BoxH), Qt::AlignCenter, QStringLiteral(":"));
    if (d->twelve) {
        // a vertical segmented AM / PM
        const QRectF period(d->minuteBox.right() + PeriodGap, Top, PeriodW, BoxH);
        d->am = QRectF(period.left(), period.top(), PeriodW, BoxH / 2);
        d->pm = QRectF(period.left(), period.center().y(), PeriodW, BoxH / 2);
        const bool afternoon = d->time.hour() >= 12;
        QPainterPath outline;
        outline.addRoundedRect(period.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        p.save();
        p.setClipPath(outline);
        p.fillRect(afternoon ? d->pm : d->am, s.tertiaryContainer);
        p.restore();
        p.setPen(QPen(s.outline, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(outline);
        p.drawLine(QPointF(period.left(), period.center().y()), QPointF(period.right(), period.center().y()));
        p.setFont(flex(16, 500));
        p.setPen(afternoon ? s.onSurfaceVariant : s.onTertiaryContainer);
        p.drawText(d->am, Qt::AlignCenter, QLocale().amText());
        p.setPen(afternoon ? s.onTertiaryContainer : s.onSurfaceVariant);
        p.drawText(d->pm, Qt::AlignCenter, QLocale().pmText());
    }

    // The dial
    d->centre = QPointF(this->width() / 2.0, Top + BoxH + Gap + Dial / 2);
    p.setPen(Qt::NoPen);
    p.setBrush(s.surfaceContainerHighest);
    p.drawEllipse(d->centre, Dial / 2, Dial / 2);

    struct Label {
            QPointF at;
            QString text;
    };
    QList<Label> labels;
    auto ring = [&](double radius, const std::function<QString(int)>& text) {
        for (int i = 0; i < 12; i++) {
            const double a = i * 30 * M_PI / 180;
            labels.append({d->centre + QPointF(radius * std::sin(a), -radius * std::cos(a)), text(i)});
        }
    };
    if (d->minutes) {
        ring(Outer, [](int i) { return QStringLiteral("%1").arg(i * 5, 2, 10, QLatin1Char('0')); });
    } else if (d->twelve) {
        ring(Outer, [](int i) { return QString::number(i == 0 ? 12 : i); });
    } else {
        ring(Outer, [](int i) { return i == 0 ? QStringLiteral("00") : QString::number(i); });
        ring(Inner, [](int i) { return QString::number(12 + i); });
    }
    auto drawLabels = [&](QColor colour) {
        p.setPen(colour);
        for (const Label& label : std::as_const(labels)) {
            const bool inner = std::hypot(label.at.x() - d->centre.x(), label.at.y() - d->centre.y()) < (Outer + Inner) / 2;
            p.setFont(flex(inner ? 14 : 16, 400));
            p.drawText(QRectF(label.at - QPointF(Handle / 2, Handle / 2), QSizeF(Handle, Handle)), Qt::AlignCenter, label.text);
        }
    };
    drawLabels(s.onSurface);

    // The selector: a line from the middle to the handle, the label under it in OnPrimary
    const double a = d->angle->currentValue().toDouble() * M_PI / 180;
    const double r = d->radius->currentValue().toDouble();
    const QPointF handle = d->centre + QPointF(r * std::sin(a), -r * std::cos(a));
    p.setPen(QPen(s.primary, 2));
    p.drawLine(d->centre, handle);
    p.setPen(Qt::NoPen);
    p.setBrush(s.primary);
    p.drawEllipse(d->centre, 4, 4);
    p.drawEllipse(handle, Handle / 2, Handle / 2);
    QPainterPath inHandle;
    inHandle.addEllipse(handle, Handle / 2, Handle / 2);
    p.save();
    p.setClipPath(inHandle);
    drawLabels(s.onPrimary);
    p.restore();
    if (d->minutes && d->time.minute() % 5 != 0) {
        // between the labels: a dot in the handle
        p.setBrush(s.onPrimary);
        p.drawEllipse(handle, 2, 2);
    }

    if (this->hasFocus() && d->keyboard) {
        // where the keys go: a focus ring round the box being set
        p.setPen(QPen(s.secondary, 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect((d->minutes ? d->minuteBox : d->hourBox).adjusted(-3, -3, 3, 3), 11, 11);
    }
}

void M3TimeDial::mousePressEvent(QMouseEvent* event) {
    d->keyboard = false;
    const QPointF at = event->position();
    if (d->hourBox.contains(at)) return setMinutesMode(false);
    if (d->minuteBox.contains(at)) return setMinutesMode(true);
    if (d->twelve && (d->am.contains(at) || d->pm.contains(at))) {
        const bool afternoon = d->pm.contains(at);
        if (afternoon != (d->time.hour() >= 12)) setTime(QTime((d->time.hour() + 12) % 24, d->time.minute()));
        return;
    }
    if (std::hypot(at.x() - d->centre.x(), at.y() - d->centre.y()) <= Dial / 2) {
        d->dragging = true;
        pick(at, false);
    }
}

void M3TimeDial::mouseMoveEvent(QMouseEvent* event) {
    if (d->dragging) pick(event->position(), false);
}

void M3TimeDial::mouseReleaseEvent(QMouseEvent* event) {
    if (!d->dragging) return;
    d->dragging = false;
    pick(event->position(), true);
}

void M3TimeDial::keyPressEvent(QKeyEvent* event) {
    d->keyboard = true;
    this->update();
    // arrows step the box being set (minutes don't carry into the hour); Space swaps boxes
    const int step = event->key() == Qt::Key_Up || event->key() == Qt::Key_Right ? 1 : event->key() == Qt::Key_Down || event->key() == Qt::Key_Left ? -1 : 0;
    if (step != 0) {
        setTime(d->minutes ? QTime(d->time.hour(), (d->time.minute() + step + 60) % 60) : d->time.addSecs(step * 3600));
        return;
    }
    if (event->key() == Qt::Key_Space) {
        setMinutesMode(!d->minutes);
        return;
    }
    QWidget::keyPressEvent(event);
}
