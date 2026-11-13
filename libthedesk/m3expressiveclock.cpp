/****************************************
 *
 *   theDesk - Material 3 Expressive clock
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3expressiveclock.h"

#include <QEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QTime>
#include <QTimer>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    QFont flex(const QFont& base, double px, double wght, double wdth) {
        QFont font = base;
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, wght, 1000.0)));
        font.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, wdth, 151.0)));
        return font;
    }

    QPainterPath glyphs(const QString& text, const QList<QFont>& fonts) {
        // Variable glyphs overlap their own contours: fill by winding
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        double x = 0;
        for (int i = 0; i < text.size(); i++) {
            path.addText(QPointF(x, 0), fonts.at(i), text.at(i));
            x += QFontMetricsF(fonts.at(i)).horizontalAdvance(text.at(i));
        }
        return path;
    }

    // "09:41", "9:41 AM", "09.41" -> hours, minutes, suffix
    struct TimeParts {
            QString hours, minutes, suffix;
    };
    TimeParts split(const QString& text) {
        TimeParts parts;
        int i = 0;
        while (i < text.size() && text.at(i).isDigit()) parts.hours += text.at(i++);
        while (i < text.size() && !text.at(i).isDigit()) i++;
        while (i < text.size() && text.at(i).isDigit()) parts.minutes += text.at(i++);
        parts.suffix = text.mid(i).trimmed();
        return parts;
    }

    constexpr double stackGap = 0.08; // of the pixel size, between hours and minutes
} // namespace

struct M3ExpressiveClockPrivate {
        int px = 96;
        M3ExpressiveClock::Style style = M3ExpressiveClock::Style::Line;
        QString text;
        QList<M3SpringAnimation*> springs; // per character: 1 = settled heavy
        QTimer timer;
};

M3ExpressiveClock::M3ExpressiveClock(QWidget* parent) :
    QWidget(parent) {
    d = new M3ExpressiveClockPrivate();
    d->timer.setInterval(1000);
    connect(&d->timer, &QTimer::timeout, this, &M3ExpressiveClock::tick);
    d->timer.start();
    tick();
}

M3ExpressiveClock::~M3ExpressiveClock() {
    delete d;
}

void M3ExpressiveClock::setPixelSize(int px) {
    d->px = px;
    this->updateGeometry();
    this->update();
}

void M3ExpressiveClock::setStyle(Style style) {
    d->style = style;
    this->updateGeometry();
    this->update();
}

void M3ExpressiveClock::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange) this->update();
    QWidget::changeEvent(event);
}

void M3ExpressiveClock::tick() {
    // Wake just after each wall-clock second so a new minute shows on time
    d->timer.start(1000 - QTime::currentTime().msec() + 5);
    QString text = QLocale().toString(QTime::currentTime(), QLocale::ShortFormat);
    if (text == d->text) return;
    while (d->springs.size() < text.size()) {
        auto* spring = new M3SpringAnimation(this);
        spring->setToken(M3Motion::SpatialDefault);
        spring->snapTo(1.0);
        connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
        d->springs.append(spring);
    }
    // Only the characters that changed dip and spring back
    for (int i = 0; i < text.size(); i++) {
        if (!d->text.isEmpty() && (i >= d->text.size() || d->text.at(i) != text.at(i))) {
            d->springs[i]->snapTo(0.0);
            d->springs[i]->animateTo(1.0);
        }
    }
    bool resized = text.size() != d->text.size();
    d->text = text;
    if (resized) this->updateGeometry();
    this->update();
}

QSize M3ExpressiveClock::sizeHint() const {
    if (d->style == Style::Stack) {
        TimeParts parts = split(d->text);
        QFont hours = flex(this->font(), d->px, 1000, 151);
        QFontMetricsF metrics(hours);
        double width = metrics.horizontalAdvance(parts.hours);
        if (!parts.suffix.isEmpty()) width += d->px * 0.06 + QFontMetricsF(flex(this->font(), d->px * 0.2, 650, 100)).horizontalAdvance(parts.suffix);
        // Two rows of figures (cap height ~0.72 px) and the gap
        return QSize(qCeil(width + d->px * 0.1), qCeil(d->px * (0.74 * 2 + stackGap) + d->px * 0.1));
    }
    QFont font = this->font();
    font.setPixelSize(d->px);
    font.setVariableAxis(QFont::Tag("wght"), 900);
    font.setVariableAxis(QFont::Tag("wdth"), 125);
    QFontMetrics metrics(font);
    return QSize(metrics.horizontalAdvance(d->text) + d->px / 4, metrics.height());
}

void M3ExpressiveClock::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (d->style == Style::Stack) {
        paintStack(&painter);
    } else {
        paintLine(&painter);
    }
}

void M3ExpressiveClock::paintStack(QPainter* painter) {
    const TimeParts parts = split(d->text);
    const int minutesStart = d->text.indexOf(parts.minutes, parts.hours.size());
    auto settle = [this](int i) {
        return d->springs.value(i) ? d->springs[i]->currentValue().toDouble() : 1.0;
    };

    // Hours: heaviest and widest; a changed digit springs back from thin
    QList<QFont> hourFonts;
    for (int i = 0; i < parts.hours.size(); i++) {
        double v = settle(i);
        hourFonts.append(flex(this->font(), d->px, 300 + 700 * v, 80 + 71 * v));
    }
    // Minutes: light, condensed, tall figures
    QList<QFont> minuteFonts;
    for (int i = 0; i < parts.minutes.size(); i++) {
        double v = settle(minutesStart + i);
        QFont font = flex(this->font(), d->px, 120 + 210 * v, 25);
        font.setVariableAxis(QFont::Tag("YTFI"), 788);
        minuteFonts.append(font);
    }
    QPainterPath hours = glyphs(parts.hours, hourFonts);
    QPainterPath minutes = glyphs(parts.minutes, minuteFonts);
    if (hours.isEmpty()) return paintLine(painter);

    const QRectF hb = hours.boundingRect();
    const double inset = d->px * 0.05;
    const QPointF hoursAt(inset - hb.left(), inset - hb.top());

    M3::Scheme scheme = M3::currentScheme();
    painter->fillPath(hours.translated(hoursAt), scheme.primary);

    if (!parts.suffix.isEmpty()) {
        QPainterPath suffix = glyphs(parts.suffix, QList<QFont>(parts.suffix.size(), flex(this->font(), d->px * 0.2, 650, 100)));
        QRectF sb = suffix.boundingRect();
        painter->fillPath(suffix.translated(inset + hb.width() + d->px * 0.06 - sb.left(), inset - sb.top()), scheme.onSurfaceVariant);
    }

    if (!minutes.isEmpty()) {
        const QRectF mb = minutes.boundingRect();
        QTransform t;
        t.translate(inset, inset + hb.height() + d->px * stackGap);
        t.scale(hb.width() / mb.width(), d->px * 0.74 / mb.height());
        t.translate(-mb.left(), -mb.top());
        painter->fillPath(t.map(minutes), scheme.tertiary);
    }
}

void M3ExpressiveClock::paintLine(QPainter* painter) {
    painter->setPen(this->palette().color(QPalette::WindowText));

    // Lay the characters out with their live weights, centred
    QList<QFont> fonts;
    double total = 0;
    for (int i = 0; i < d->text.size(); i++) {
        double v = d->springs.value(i) ? d->springs[i]->currentValue().toDouble() : 1.0;
        QFont font = this->font();
        font.setPixelSize(d->px);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8, d->px, 144)));
        font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, 300 + 600 * v, 1000.0)));
        font.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, 80 + 45 * v, 151.0)));
        fonts.append(font);
        total += QFontMetricsF(font).horizontalAdvance(d->text.at(i));
    }
    double x = 0;
    double baseline = (this->height() + QFontMetricsF(fonts.value(0, this->font())).ascent() - QFontMetricsF(fonts.value(0, this->font())).descent()) / 2;
    for (int i = 0; i < d->text.size(); i++) {
        painter->setFont(fonts[i]);
        painter->drawText(QPointF(x, baseline), d->text.at(i));
        x += QFontMetricsF(fonts[i]).horizontalAdvance(d->text.at(i));
    }
}
