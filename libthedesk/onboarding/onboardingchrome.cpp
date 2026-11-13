/****************************************
 *
 *   theDesk - onboarding: the Pixel-style page chrome
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "onboardingchrome.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <cmath>
#include <m3motion.h>
#include <m3scheme.h>
#include <memory>
#include <onboardingmanager.h>
#include <onboardingpage.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>
#include <ttitlelabel.h>

namespace {
    enum Role {
        Primary,
        Tertiary,
        Secondary
    };

    struct Look {
            const char* shape;
            const char* symbol; // a Material Symbols ligature
            Role role;
    };

    Look lookFor(const QString& page) {
        if (page == QLatin1String("OnboardingRegion")) return {"Cookie9Sided", "language", Primary};
        if (page == QLatin1String("OnboardingTimeZone")) return {"Clover4Leaf", "schedule", Secondary};
        if (page == QLatin1String("OnboardingNetwork")) return {"Cookie6Sided", "wifi", Tertiary};
        if (page == QLatin1String("OnboardingUsers")) return {"Cookie12Sided", "group", Secondary};
        if (page == QLatin1String("OnboardingYou")) return {"Cookie4Sided", "face", Primary};
        if (page == QLatin1String("OnboardingTheme")) return {"Flower", "palette", Tertiary};
        if (page == QLatin1String("OnboardingTips")) return {"SoftBurst", "lightbulb", Primary};
        if (page == QLatin1String("OnboardingShortcuts")) return {"Cookie12Sided", "keyboard", Tertiary};
        if (page == QLatin1String("OnboardingBetaThankYou")) return {"Heart", "favorite", Tertiary};
        return {"Circle", "settings", Primary};
    }

    const m3shapes::RoundedPolygon& shapeNamed(const char* name) {
        for (const auto& named : MaterialShapes::all()) {
            if (qstrcmp(named.name, name) == 0) return named.shape();
        }
        return MaterialShapes::circle();
    }

    QColor container(const M3::Scheme& s, Role role) {
        return role == Tertiary ? s.tertiaryContainer : role == Secondary ? s.secondaryContainer : s.primaryContainer;
    }

    QColor onContainer(const M3::Scheme& s, Role role) {
        return role == Tertiary ? s.onTertiaryContainer : role == Secondary ? s.onSecondaryContainer : s.onPrimaryContainer;
    }

    QColor mix(const QColor& a, const QColor& b, double k) {
        k = qBound(0.0, k, 1.0);
        return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * k), float(a.greenF() + (b.greenF() - a.greenF()) * k),
            float(a.blueF() + (b.blueF() - a.blueF()) * k));
    }

    // A Material Symbol (filled, by its ligature name) centred on a point
    void drawSymbol(QPainter& painter, const QString& symbol, QPointF centre, double px, QColor colour) {
        if (symbol.isEmpty() || px < 1) return;
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qMax(1, int(px)));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        painter.setFont(font);
        painter.setPen(colour);
        painter.drawText(QRectF(centre.x() - px, centre.y() - px, 2 * px, 2 * px), Qt::AlignCenter, symbol);
    }

    QBoxLayout* layoutHolding(QLayout* layout, QWidget* widget) {
        if (!layout) return nullptr;
        for (int i = 0; i < layout->count(); i++) {
            QLayoutItem* item = layout->itemAt(i);
            if (item->widget() == widget) return qobject_cast<QBoxLayout*>(layout);
            if (auto* found = layoutHolding(item->layout(), widget)) return found;
        }
        return nullptr;
    }
} // namespace

// ---- The hero -------------------------------------------------------------

struct OnboardingHeroPrivate {
        Look from{"Circle", "", Primary};
        Look to{"Circle", "", Primary};
        std::unique_ptr<m3shapes::Morph> morph;
        M3SpringAnimation* morphSpring;
        M3SpringAnimation* shownSpring;
};

OnboardingHero::OnboardingHero(QWidget* parent) :
    QWidget(parent) {
    d = new OnboardingHeroPrivate();
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setFixedSize(size, size);

    d->morphSpring = new M3SpringAnimation(this);
    d->morphSpring->setToken(M3Motion::SpatialDefault); // expressive: overshoots a touch
    d->morphSpring->setStartValue(1.0);
    connect(d->morphSpring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));

    d->shownSpring = new M3SpringAnimation(this);
    d->shownSpring->setToken(M3Motion::SpatialFast);
    d->shownSpring->setStartValue(0.0);
    connect(d->shownSpring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

OnboardingHero::~OnboardingHero() {
    delete d;
}

void OnboardingHero::showStep(const QString& pageName) {
    if (!OnboardingChrome::hasHero(pageName)) {
        d->shownSpring->animateTo(0.0);
        return;
    }
    const Look look = lookFor(pageName);
    const bool wasShown = d->shownSpring->currentValue().toDouble() > 0.05;
    if (!wasShown) {
        // Arriving from the welcome: grows in as this step's shape, spinning
        d->from = look;
        d->to = look;
        d->morph.reset();
        d->morphSpring->snapTo(0.0);
        d->morphSpring->animateTo(1.0);
    } else if (qstrcmp(look.shape, d->to.shape) != 0 || qstrcmp(look.symbol, d->to.symbol) != 0) {
        // From the last step's shape into this one's
        const double k = qBound(0.0, d->morphSpring->currentValue().toDouble(), 1.0);
        d->from = k < 0.5 ? d->from : d->to;
        d->to = look;
        d->morph = std::make_unique<m3shapes::Morph>(shapeNamed(d->from.shape), shapeNamed(d->to.shape));
        d->morphSpring->snapTo(0.0);
        d->morphSpring->animateTo(1.0);
    }
    d->shownSpring->animateTo(1.0);
    this->raise();
}

void OnboardingHero::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const double shown = d->shownSpring->currentValue().toDouble();
    if (shown <= 0.01) return;
    const double k = d->morphSpring->currentValue().toDouble(); // may overshoot
    const double km = qBound(0.0, k, 1.0);
    const M3::Scheme& s = M3::currentScheme();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QPointF centre = QRectF(this->rect()).center();
    const double side = size * qMax(0.0, shown) * 0.98;
    const QRectF box(-side / 2, -side / 2, side, side);

    // The shape: morphing, a quarter turn as it goes (the overshoot sways it)
    QPainterPath path = d->morph ? m3shapes::toPath(*d->morph, float(km), box) : m3shapes::toPath(shapeNamed(d->to.shape), box);
    QTransform transform;
    transform.translate(centre.x(), centre.y());
    transform.rotate((1 - k) * -90);
    painter.fillPath(transform.map(path), mix(container(s, d->from.role), container(s, d->to.role), km));

    // The symbol: the old one shrinks away, the new one springs in
    const double symbolPx = 36 * qMax(0.0, shown);
    if (d->morph && km < 0.5) {
        QColor c = onContainer(s, d->from.role);
        c.setAlphaF(float(1 - km * 2));
        drawSymbol(painter, QString::fromLatin1(d->from.symbol), centre, symbolPx * (1 - 0.5 * km), c);
    } else {
        const double in = d->morph ? (k - 0.5) * 2 : k;
        QColor c = onContainer(s, d->to.role);
        c.setAlphaF(float(qBound(0.0, in * 1.5, 1.0)));
        drawSymbol(painter, QString::fromLatin1(d->to.symbol), centre, symbolPx * (0.6 + 0.4 * qMax(0.0, in)), c);
    }
}

// ---- The progress line ------------------------------------------------------

struct OnboardingProgressPrivate {
        M3SpringAnimation* spring;
        QTimer frame;
        QElapsedTimer clock;
};

OnboardingProgress::OnboardingProgress(QWidget* parent) :
    QWidget(parent) {
    d = new OnboardingProgressPrivate();
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    d->spring = new M3SpringAnimation(this);
    d->spring->setToken(M3Motion::SpatialDefault);
    d->spring->setStartValue(0.0);
    connect(d->spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    d->frame.setInterval(33); // the waves drift
    connect(&d->frame, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    d->clock.start();
}

OnboardingProgress::~OnboardingProgress() {
    delete d;
}

void OnboardingProgress::setProgress(double progress) {
    d->spring->animateTo(qBound(0.0, progress, 1.0));
}

void OnboardingProgress::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    d->frame.start();
}

void OnboardingProgress::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->frame.stop();
}

void OnboardingProgress::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    // M3E wavy progress (ProgressIndicator tokens): 3 dp waves 40 dp long,
    // a 4 dp stroke, a 4 dp gap before the track, a stop dot at its end
    const M3::Scheme& s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double amplitude = 3, wavelength = 40, stroke = 4, gap = 4;
    const QRectF bar = QRectF(this->rect()).adjusted(stroke, 0, -stroke, 0);
    const double y = bar.center().y();
    const double progress = qBound(0.0, d->spring->currentValue().toDouble(), 1.0);
    const double end = bar.left() + bar.width() * progress;
    const double offset = std::fmod(d->clock.elapsed() / 1000.0 * 24, wavelength);

    if (progress > 0.005) {
        QPainterPath wave;
        // The wave flattens into the line's ends
        for (double x = bar.left(); x <= end; x += 1) {
            const double ramp = qMin(1.0, qMin(x - bar.left(), end - x) / 12);
            const double yy = y + amplitude * ramp * std::sin(2 * M_PI * (x - bar.left() - offset) / wavelength);
            if (x == bar.left()) wave.moveTo(x, yy);
            else wave.lineTo(x, yy);
        }
        p.setPen(QPen(s.primary, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(wave);
    }
    const double trackStart = end + gap + stroke;
    if (trackStart < bar.right()) {
        p.setPen(QPen(s.secondaryContainer, stroke, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(trackStart, y), QPointF(bar.right(), y));
        // The stop dot
        p.setPen(Qt::NoPen);
        p.setBrush(s.primary);
        p.drawEllipse(QPointF(bar.right(), y), stroke / 2, stroke / 2);
    }
}

// ---- The pages --------------------------------------------------------------

OnboardingChrome::StepLook OnboardingChrome::stepLook(const QString& pageName) {
    if (pageName == QLatin1String("OnboardingWelcome")) return {"Circle", "waving_hand", 0};
    if (pageName == QLatin1String("OnboardingFinal")) return {"Sunny", "check", 0};
    const Look look = lookFor(pageName);
    return {look.shape, look.symbol, int(look.role)};
}

bool OnboardingChrome::hasHero(const QString& pageName) {
    return pageName != QLatin1String("OnboardingWelcome") && pageName != QLatin1String("OnboardingFinal");
}

void OnboardingChrome::stylePage(OnboardingPage* page) {
    if (!page || page->property("m3ChromeStyled").toBool()) return;
    page->setProperty("m3ChromeStyled", true);
    QLayout* layout = page->layout();
    if (!layout) return;

    // Room for the hero, everything inset like Pixel's setup
    layout->setContentsMargins(margin, margin + heroSpace, margin, 16);

    // The headline: flush with the hero, bigger, no back arrow (Back is at
    // the bottom now)
    if (auto* title = page->findChild<tTitleLabel*>(QStringLiteral("titleLabel"))) {
        title->setBackButtonShown(false);
        title->setMargin(0);
        title->setMinimumHeight(58);
        title->setProperty("m3OnboardingTitle", 44);
    }

    // The body: BodyLarge in OnSurfaceVariant
    QLabel* body = page->findChild<QLabel*>(QStringLiteral("descriptionLabel"));
    if (!body) {
        body = page->findChild<QLabel*>(QStringLiteral("label"));
    }
    if (body) {
        body->setWordWrap(true);
        QFont font = body->font();
        font.setPixelSize(16);
        body->setFont(font);
        body->setMargin(0);
        QPalette palette = body->palette();
        palette.setColor(QPalette::WindowText, M3::currentScheme().onSurfaceVariant);
        body->setPalette(palette);
    }

    // No rules
    for (QFrame* frame : page->findChildren<QFrame*>()) {
        if (frame->frameShape() == QFrame::HLine || frame->frameShape() == QFrame::VLine) frame->setVisible(false);
    }

    // The bottom bar: a text Back on the left, the page's Next filled
    if (auto* next = page->findChild<QPushButton*>(QStringLiteral("nextButton"))) {
        next->setProperty("m3ButtonStyle", QStringLiteral("filled"));
        QBoxLayout* row = layoutHolding(layout, next);
        if (!row && next->parentWidget()) row = layoutHolding(next->parentWidget()->layout(), next);
        if (row) {
            auto* back = new QPushButton(QCoreApplication::translate("OnboardingChrome", "Back"), next->parentWidget());
            back->setObjectName(QStringLiteral("m3BackButton"));
            back->setProperty("m3ButtonStyle", QStringLiteral("text"));
            QObject::connect(back, &QPushButton::clicked, back, [] {
                StateManager::onboardingManager()->previousStep();
            });
            row->insertWidget(0, back);
            if (row->indexOf(next) == 1) row->insertStretch(1);
        }
    }
}
