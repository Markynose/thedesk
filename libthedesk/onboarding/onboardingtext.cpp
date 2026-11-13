/****************************************
 *
 *   theDesk - onboarding: expressive, animated text
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "onboardingtext.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QLabel>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include <QTimer>
#include <cmath>
#include <m3scheme.h>

namespace {
    // Step response of an M3 spring (0 at t <= 0; t in seconds)
    double spring(double t, double damping, double stiffness) {
        if (t <= 0) return 0;
        const double w = std::sqrt(stiffness);
        if (damping >= 1) return 1 - std::exp(-w * t) * (1 + w * t);
        const double wd = w * std::sqrt(1 - damping * damping);
        return 1 - std::exp(-damping * w * t) * (std::cos(wd * t) + damping * w / wd * std::sin(wd * t));
    }
    double spatial(double t) { return spring(t, 0.8, 380); } // SpatialDefault
    double bouncy(double t) { return spring(t, 0.5, 520); }  // expressive: overshoots the axes
    double effects(double t) { return spring(t, 1.0, 800); } // EffectsSlow: never overshoots

    QColor mix(QColor a, QColor b, double k) {
        k = qBound(0.0, k, 1.0);
        return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * k), float(a.greenF() + (b.greenF() - a.greenF()) * k),
            float(a.blueF() + (b.blueF() - a.blueF()) * k), float(a.alphaF() + (b.alphaF() - a.alphaF()) * k));
    }

    QStringList graphemes(const QString& text) {
        QStringList out;
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
        int start = 0;
        while (finder.toNextBoundary() != -1) {
            out.append(text.mid(start, finder.position() - start));
            start = finder.position();
        }
        return out;
    }

    constexpr double titleStagger = 0.032; // s between letters
    constexpr double bodyStagger = 0.07;   // s between lines
} // namespace

struct OnboardingTextPrivate {
        QLabel* label;
        OnboardingText::Kind kind;
        int pixelSize;
        QElapsedTimer clock;
        qint64 playAt = -100000; // ms on the clock; far back = settled
        qint64 jumpAt = -100000;
        QTimer frame;
        int lines = 1; // the body's lines, from the last layout

        qint64 since() const {
            return clock.elapsed() - playAt;
        }
        // How long the animation runs, in ms after playAt
        qint64 duration() const {
            if (kind == OnboardingText::Title) return qint64((label->text().size() * titleStagger + 1.3) * 1000);
            return qint64((lines * bodyStagger + 0.9) * 1000);
        }
        QFont font(double wght, double wdth) const {
            QFont font(QStringLiteral("Roboto Flex"));
            font.setPixelSize(pixelSize);
            font.setVariableAxis(QFont::Tag("opsz"), float(qMin(pixelSize, 144)));
            font.setVariableAxis(QFont::Tag("wght"), float(wght));
            font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
            return font;
        }
};

OnboardingText::OnboardingText(QLabel* label, Kind kind, int pixelSize) :
    QObject(label) {
    d = new OnboardingTextPrivate();
    d->label = label;
    d->kind = kind;
    d->pixelSize = pixelSize;
    d->clock.start();
    d->frame.setInterval(16);
    d->frame.setTimerType(Qt::PreciseTimer);
    connect(&d->frame, &QTimer::timeout, this, [this] {
        const qint64 since = d->since();
        const bool jumping = d->clock.elapsed() - d->jumpAt < 1500;
        if (since > d->duration() && !jumping) d->frame.stop();
        d->label->update();
    });
    if (kind == Title) {
        // Room for the settled title; the letters are drawn here (eventFilter)
        label->setFont(d->font(600, 100));
        label->setCursor(Qt::PointingHandCursor);
    }
    label->installEventFilter(this);
}

OnboardingText::~OnboardingText() {
    delete d;
}

OnboardingText* OnboardingText::forLabel(QLabel* label, Kind kind, int pixelSize) {
    if (auto* existing = label->findChild<OnboardingText*>(QString(), Qt::FindDirectChildrenOnly)) return existing;
    return new OnboardingText(label, kind, pixelSize);
}

namespace {
    QPair<QLabel*, QLabel*> pageLabels(QWidget* page) {
        QLabel* title = nullptr;
        const auto labels = page->findChildren<QLabel*>();
        for (QLabel* label : labels) {
            if (label->property("m3OnboardingTitle").isValid()) title = label;
        }
        if (!title) title = page->findChild<QLabel*>(QStringLiteral("titleLabel"));
        QLabel* body = page->findChild<QLabel*>(QStringLiteral("descriptionLabel"));
        if (!body) {
            QLabel* label = page->findChild<QLabel*>(QStringLiteral("label"));
            if (label && label != title && label->wordWrap()) body = label;
        }
        return {title, body};
    }
} // namespace

void OnboardingText::preparePage(QWidget* page) {
    if (!page) return;
    const auto [title, body] = pageLabels(page);
    if (title) {
        const int size = title->property("m3OnboardingTitle").toInt();
        forLabel(title, Title, size > 0 ? size : 28)->hide();
    }
    if (body) forLabel(body, Body)->hide();
}

void OnboardingText::hide() {
    d->frame.stop();
    d->playAt = d->clock.elapsed() + 1000000000;
    d->label->update();
}

void OnboardingText::animatePage(QWidget* page, int delayMs) {
    if (!page) return;
    const auto [title, body] = pageLabels(page);
    if (title) {
        const int size = title->property("m3OnboardingTitle").toInt();
        forLabel(title, Title, size > 0 ? size : 28)->play(delayMs);
    }
    if (body) forLabel(body, Body)->play(delayMs + 160);
}

void OnboardingText::play(int delayMs) {
    d->playAt = d->clock.elapsed() + delayMs;
    d->frame.start();
    d->label->update();
}

bool OnboardingText::eventFilter(QObject* watched, QEvent* event) {
    if (watched != d->label) return false;
    if (event->type() == QEvent::Paint) {
        if (d->kind == Title) {
            paintTitle();
            return true;
        }
        return paintBody();
    }
    if (event->type() == QEvent::MouseButtonPress && d->kind == Title) {
        // The letters jump in a wave and get heavier for a moment
        d->jumpAt = d->clock.elapsed();
        d->frame.start();
        return true;
    }
    return false;
}

void OnboardingText::paintTitle() {
    QLabel* label = d->label;
    QPainter p(label);
    p.setRenderHint(QPainter::Antialiasing);
    const M3::Scheme& scheme = M3::currentScheme();
    const QRectF r = QRectF(label->contentsRect()).adjusted(label->margin(), label->margin(), -label->margin(), -label->margin());
    const double since = d->since() / 1000.0;
    const double jumpSince = (d->clock.elapsed() - d->jumpAt) / 1000.0;

    // Step headlines (40 px and up) settle heavier and a little wider: the
    // expressive emphasized headline; smaller titles as before
    const bool emphasized = d->pixelSize >= 40;
    const double endWght = emphasized ? 760 : 600, endWdth = emphasized ? 108 : 100;
    const QFontMetricsF settled(d->font(endWght, endWdth));
    const double baseline = r.top() + (r.height() - settled.ascent() - settled.descent()) / 2 + settled.ascent();

    // Each letter, as it is now
    struct Letter {
            QPainterPath glyph;
            QColor colour;
    };
    QList<Letter> letters;
    double x = 0;
    const QStringList clusters = graphemes(label->text());
    for (int i = 0; i < clusters.size(); i++) {
        const double t = since - i * titleStagger;
        const double k = bouncy(t);
        const double wdth = 25 + (endWdth - 25) * qBound(0.0, k, 1.2);
        double wght = 200 + (endWght - 200) * qBound(0.0, k, 1.15);
        double lift = (1 - qMin(k, 1.0)) * -0.45 * d->pixelSize; // rises into place
        const double jt = jumpSince - i * 0.045;
        if (jt > 0 && jt < 1.4) {
            lift += 0.3 * d->pixelSize * std::exp(-4 * jt) * std::sin(jt * 14);
            wght = qMin(1000.0, wght + 300 * std::exp(-5 * jt));
        }
        const QFont font = d->font(wght, wdth);
        QPainterPath glyph;
        glyph.setFillRule(Qt::WindingFill);
        glyph.addText(x, -lift, font, clusters.at(i));
        // Primary as it arrives, settling to OnSurface; fading in, never overshooting
        QColor colour = mix(scheme.primary, scheme.onSurface, effects(t - 0.25));
        colour.setAlphaF(float(qBound(0.0, effects(t * 1.6), 1.0)));
        letters.append({glyph, colour});
        x += QFontMetricsF(font).horizontalAdvance(clusters.at(i));
    }

    double left = r.left();
    const Qt::Alignment align = QStyle::visualAlignment(label->layoutDirection(), label->alignment());
    if (align & Qt::AlignHCenter) left = r.center().x() - x / 2;
    else if (align & Qt::AlignRight) left = r.right() - x;
    for (const Letter& letter : letters) p.fillPath(letter.glyph.translated(left, baseline), letter.colour);
}

bool OnboardingText::paintBody() {
    QLabel* label = d->label;
    const double since = d->since() / 1000.0;
    if (d->since() > d->duration() || label->text().isEmpty() || Qt::mightBeRichText(label->text())) return false;

    const QRectF r = QRectF(label->contentsRect()).adjusted(label->margin(), label->margin(), -label->margin(), -label->margin());
    const Qt::Alignment align = QStyle::visualAlignment(label->layoutDirection(), label->alignment());
    QTextLayout layout(label->text(), label->font());
    QTextOption option(align & Qt::AlignHorizontal_Mask);
    option.setWrapMode(label->wordWrap() ? QTextOption::WordWrap : QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    double height = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(r.width());
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    d->lines = qMax(1, layout.lineCount());

    double top = r.top() + (r.height() - height) / 2;
    if (align & Qt::AlignTop) top = r.top();
    else if (align & Qt::AlignBottom) top = r.bottom() - height;

    // Line by line: rising a little (spatial) and fading in (effects)
    QPainter p(label);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(label->palette().color(label->foregroundRole()));
    for (int i = 0; i < layout.lineCount(); i++) {
        const double t = since - i * bodyStagger;
        p.setOpacity(qBound(0.0, effects(t), 1.0));
        layout.lineAt(i).draw(&p, QPointF(r.left(), top + (1 - spatial(t)) * 12));
    }
    return true;
}
