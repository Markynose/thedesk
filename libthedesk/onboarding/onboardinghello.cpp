/****************************************
 *
 *   theDesk - onboarding: the "Hello" welcome
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "onboardinghello.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTimer>
#include <QtMath>
#include <memory>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>

namespace {
    // Step response of an M3 spring (from 0 at t <= 0, seconds)
    double spring(double t, double damping, double stiffness) {
        if (t <= 0) return 0;
        const double w = std::sqrt(stiffness);
        if (damping >= 1) return 1 - std::exp(-w * t) * (1 + w * t);
        const double wd = w * std::sqrt(1 - damping * damping);
        return 1 - std::exp(-damping * w * t) * (std::cos(wd * t) + damping * w / wd * std::sin(wd * t));
    }
    double spatial(double t) { return spring(t, 0.8, 380); } // SpatialDefault
    double bouncy(double t) { return spring(t, 0.45, 600); } // the expressive "fast spatial", bouncier
    double smooth(double v) {
        v = qBound(0.0, v, 1.0);
        return v * v * (3 - 2 * v);
    }

    enum Role {
        PrimaryContainer,
        TertiaryContainer,
        SecondaryContainer,
        Primary,
        Tertiary,
        Secondary,
        RoleCount
    };

    QColor roleColour(const M3::Scheme& s, int role) {
        switch (role % RoleCount) {
            case PrimaryContainer:
                return s.primaryContainer;
            case TertiaryContainer:
                return s.tertiaryContainer;
            case SecondaryContainer:
                return s.secondaryContainer;
            case Primary:
                return s.primary;
            case Tertiary:
                return s.tertiary;
            default:
                return s.secondary;
        }
    }

    int shapeIndex(const char* name) {
        const auto& all = MaterialShapes::all();
        for (int i = 0; i < int(all.size()); i++) {
            if (qstrcmp(all[i].name, name) == 0) return i;
        }
        return 0;
    }

    // A shape around the edges: where (fractions of the widget), how big
    // (a fraction of its height), its colour role, its first shape
    struct Slot {
            QPointF at;
            double radius;
            int role;
            const char* shape;
            double rotation;
    };
    const Slot shapeSlots[] = {
        {{0.90, 0.08}, 0.20, PrimaryContainer, "Cookie12Sided", 10},
        {{0.50, 1.06}, 0.12, TertiaryContainer, "Sunny", 0},
        {{0.99, 0.58}, 0.11, SecondaryContainer, "Pill", -30},
        {{0.72, 0.20}, 0.04, Primary, "Flower", 0},
        {{0.56, 0.86}, 0.03, Tertiary, "Gem", 15},
        {{0.94, 0.80}, 0.03, Secondary, "Circle", 0},
    };
    constexpr int slotCount = int(std::size(shapeSlots));

    struct ShapeState {
            int from, to;            // MaterialShapes::all() indexes
            std::shared_ptr<m3shapes::Morph> morph;
            qint64 morphedAt = -100000; // ms
            qint64 poppedAt = -100000;  // a click: squash and spring back
            int role;
            double rotation;
    };

    const QStringList greetings = {
        QStringLiteral("Hello"), QStringLiteral("Привіт"), QStringLiteral("Hola"), QStringLiteral("Bonjour"),
        QStringLiteral("Ciao"), QStringLiteral("Hallo"), QStringLiteral("Olá"), QStringLiteral("Cześć"), QStringLiteral("Hej")};

    const QList<int> konami = {Qt::Key_Up, Qt::Key_Up, Qt::Key_Down, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right, Qt::Key_B, Qt::Key_A};

} // namespace

struct OnboardingHelloPrivate {
        QTimer frame;
        QElapsedTimer clock;
        M3::Scheme scheme;
        QRectF greetingRect;
        ShapeState shapes[slotCount];

        // The greeting
        int greeting = 0;
        QString word = greetings.first();
        QString previousWord;
        qint64 wordAt = 0;        // when the current word started coming in
        qint64 jumpAt = -100000;  // a click on the word: the letters jump
        QList<qint64> recentClicks;
        qint64 expressiveUntil = -1; // the "M3 Expressive" egg
        bool held = false;           // the opening is playing here
        // Taking over from the opening's word: a wave of weight and width
        // runs through it and each letter swaps to the greeting's at the top
        QString swapFrom;
        qint64 swapAt = -1;

        // The Konami code
        int konamiProgress = 0;
        qint64 partyUntil = -1;
        qint64 lastPartyMorph = 0;

        qint64 now() const {
            // A moment after showing: the onboarding appears as the loading
            // screen's exit opens onto it
            return clock.isValid() ? clock.elapsed() - 500 : 0;
        }
};

OnboardingHello::OnboardingHello(QWidget* parent) :
    QWidget(parent) {
    d = new OnboardingHelloPrivate();
    d->scheme = M3::currentScheme();
    for (int i = 0; i < slotCount; i++) {
        d->shapes[i].from = d->shapes[i].to = shapeIndex(shapeSlots[i].shape);
        d->shapes[i].role = shapeSlots[i].role;
        d->shapes[i].rotation = shapeSlots[i].rotation;
    }
    d->frame.setInterval(16);
    d->frame.setTimerType(Qt::PreciseTimer);
    connect(&d->frame, &QTimer::timeout, this, &OnboardingHello::tick);
    this->setCursor(Qt::ArrowCursor);
}

OnboardingHello::~OnboardingHello() {
    delete d;
}

void OnboardingHello::setGreetingRect(QRectF rect) {
    d->greetingRect = rect;
    this->update();
}

QSize OnboardingHello::greetingSize() const {
    return QSize(440, 130);
}

void OnboardingHello::holdGreeting() {
    d->held = true;
    this->update();
}

namespace {
    constexpr qint64 swapMs = 1150;
}

void OnboardingHello::releaseGreeting(const QString& from) {
    d->held = false;
    d->swapFrom = from;
    d->swapAt = d->now();
    d->previousWord.clear();
    d->greeting = 0;
    d->word = greetings.first();
    d->wordAt = d->swapAt + swapMs; // its 2.8 s start once the swap is done
    this->update();
}

void OnboardingHello::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    d->scheme = M3::currentScheme();
    d->clock.start();
    d->wordAt = 0;
    d->frame.start();
    qApp->installEventFilter(this); // the Konami code, wherever the focus is
}

void OnboardingHello::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->frame.stop();
    qApp->removeEventFilter(this);
}

void OnboardingHello::tick() {
    const qint64 now = d->now();
    if (d->held) d->wordAt = now; // (waiting for the opening)
    // Next greeting every 2.8 s (not while the "M3 Expressive" egg shows)
    if (!d->held && now > d->expressiveUntil && now - d->wordAt > 2800) {
        d->swapAt = -1;
        d->previousWord = d->word;
        d->greeting = (d->greeting + 1) % greetings.size();
        d->word = greetings.at(d->greeting);
        d->wordAt = now;
    } else if (d->expressiveUntil > 0 && now > d->expressiveUntil) {
        d->expressiveUntil = -1;
        d->previousWord = d->word;
        d->word = greetings.at(d->greeting);
        d->wordAt = now;
    }

    // The party: a shape morphs every 300 ms, colours turn
    if (now < d->partyUntil && now - d->lastPartyMorph > 300) {
        d->lastPartyMorph = now;
        const int i = QRandomGenerator::global()->bounded(slotCount);
        ShapeState& shape = d->shapes[i];
        shape.from = shape.to;
        shape.to = (shape.to + 1 + QRandomGenerator::global()->bounded(5)) % int(MaterialShapes::all().size());
        shape.morph = std::make_shared<m3shapes::Morph>(MaterialShapes::all()[shape.from].shape(), MaterialShapes::all()[shape.to].shape());
        shape.morphedAt = now;
        shape.role = (shape.role + 1) % RoleCount;
    }
    this->update();
}

void OnboardingHello::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qint64 now = d->now();
    const double h = this->height(), w = this->width();
    const bool party = now < d->partyUntil;
    // Within the card's rounded corners
    QPainterPath card;
    card.addRoundedRect(QRectF(this->rect()), 28, 28);
    p.setClipPath(card);

    // ---- The shapes, springing in one after another
    for (int i = 0; i < slotCount; i++) {
        const Slot& slot = shapeSlots[i];
        ShapeState& shape = d->shapes[i];
        const double radius = slot.radius * h;
        const double grow = bouncy((now - 120 - i * 90) / 1000.0);
        if (grow <= 0.001) continue;
        const double morph = shape.morph ? qBound(0.0, spatial((now - shape.morphedAt) / 1000.0), 1.0) : 1;
        const double pop = 1 - 0.18 * std::exp(-6 * (now - shape.poppedAt) / 1000.0) * std::cos(18 * (now - shape.poppedAt) / 1000.0);
        const double scale = grow * (now - shape.poppedAt < 1500 ? pop : 1);
        double rotation = shape.rotation + (1 - grow) * 50 + (party ? (now % 4000) / 4000.0 * 360 * (i % 2 ? -1 : 1) : 0);
        const QRectF box(-radius, -radius, 2 * radius, 2 * radius);
        QPainterPath path = shape.morph && morph < 1 ? m3shapes::toPath(*shape.morph, float(morph), box)
                                                     : m3shapes::toPath(MaterialShapes::all()[shape.to].shape(), box);
        QTransform t;
        t.translate(slot.at.x() * w, slot.at.y() * h);
        t.rotate(rotation);
        t.scale(scale, scale);
        p.fillPath(t.map(path), roleColour(d->scheme, shape.role));
    }

    // ---- The greeting
    if (!d->greetingRect.isValid() || d->held) return;
    const bool expressive = now < d->expressiveUntil;
    const double px = expressive ? 64 : 104;
    auto drawWord = [&](const QString& word, double since, bool leaving) {
        // Each letter springs in (or squeezes out), a little after the one before
        double x = 0;
        QList<QPair<QPainterPath, double>> letters;
        for (int i = 0; i < word.size(); i++) {
            const double t = since / 1000.0 - i * 0.045;
            double k = leaving ? 1 - qBound(0.0, t / 0.22, 1.0) : bouncy(t);
            if (k <= 0.002) {
                x += px * 0.35;
                continue;
            }
            const double wdth = 25 + (expressive ? 126 : 100) * qBound(0.0, k, 1.1);
            double wght = 250 + (expressive ? 750 : 450) * qBound(0.0, k, 1.0);
            // A click: the letters jump in a wave and get heavier for a moment
            const double jt = (now - d->jumpAt) / 1000.0 - i * 0.05;
            double lift = 0;
            if (jt > 0 && jt < 1.2) {
                lift = 26 * std::exp(-4 * jt) * std::sin(jt * 14);
                wght = qMin(1000.0, wght + 300 * std::exp(-5 * jt));
            }
            QFont font(QStringLiteral("Roboto Flex"));
            font.setPixelSize(qMax(1, int(px)));
            font.setVariableAxis(QFont::Tag("opsz"), float(qMin(px, 144.0)));
            font.setVariableAxis(QFont::Tag("wght"), float(wght));
            font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
            QPainterPath glyph;
            glyph.setFillRule(Qt::WindingFill);
            glyph.addText(0, 0, font, word.mid(i, 1));
            QFontMetricsF metrics(font);
            letters.append({glyph.translated(x, -lift - (1 - qMin(k, 1.0)) * 24), qBound(0.0, k, 1.0)});
            x += metrics.horizontalAdvance(word.at(i));
        }
        const QRectF r = d->greetingRect;
        const double baseline = r.top() + r.height() * 0.78;
        const QColor colour = expressive ? d->scheme.tertiary : d->scheme.primary;
        for (const auto& [glyph, alpha] : letters) {
            QColor c = colour;
            c.setAlphaF(alpha);
            p.fillPath(glyph.translated(r.left(), baseline), c);
        }
    };
    if (d->swapAt >= 0 && now - d->swapAt < swapMs) {
        // The swap from the opening's word: rest layouts of both words, one
        // bump of weight and width travelling left to right; each slot shows
        // the old letter until the bump's top, then the new one, sliding to
        // the new word's spacing; letters with no partner squeeze away
        auto font = [&](double wght, double wdth, double ytas = 750) {
            QFont f(QStringLiteral("Roboto Flex"));
            f.setPixelSize(int(px));
            f.setVariableAxis(QFont::Tag("opsz"), float(qMin(px, 144.0)));
            f.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, wght, 1000.0)));
            f.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, wdth, 151.0)));
            f.setVariableAxis(QFont::Tag("YTAS"), float(qBound(649.0, ytas, 854.0)));
            return f;
        };
        const QString& from = d->swapFrom;
        const QString& to = d->word;
        const QRectF r = d->greetingRect;
        const double baseline = r.top() + r.height() * 0.78;
        // Laid out as it's shown: each slot's width eases from the old
        // letter's to the new one's around its swap, so the letters push
        // each other along and never overlap
        double x = 0;
        for (int i = 0; i < qMax(from.size(), to.size()); i++) {
            const double lt = (now - d->swapAt) / 1000.0 - i * 0.075;
            const double b = lt > 0 && lt < 0.38 ? std::sin(M_PI * lt / 0.38) : 0;
            // the bump: heavier, taller ascenders, lifted, a touch narrower
            const QFont bumpFont = font(700 + 260 * b, 125 - 18 * b, 750 + 100 * b);
            const QFontMetricsF metrics(bumpFont);
            const bool hasOld = i < from.size(), hasNew = i < to.size();
            const double oldAdv = hasOld ? metrics.horizontalAdvance(from.at(i)) : 0;
            const double newAdv = hasNew ? metrics.horizontalAdvance(to.at(i)) : 0;
            // (the slot reaches the new letter's width just as it swaps in)
            const double ease = smooth((lt - 0.04) / 0.15);
            double adv = oldAdv + (newAdv - oldAdv) * ease;
            const bool swapped = lt >= 0.19;
            QString ch;
            double scale = 1;
            if (hasOld && !hasNew) {
                // no partner: squeezes away into the word's end, its room with it
                ch = from.mid(i, 1);
                scale = 1 - smooth((lt - 0.06) / 0.3);
                adv = oldAdv * scale;
            } else if (!swapped) {
                ch = hasOld ? from.mid(i, 1) : QString();
                if (!hasOld) scale = 0;
            } else {
                ch = to.mid(i, 1);
                if (!hasOld) scale = qBound(0.0, bouncy(lt - 0.19), 1.1); // new letters spring in
            }
            if (!ch.isEmpty() && scale > 0.01) {
                QPainterPath glyph;
                glyph.setFillRule(Qt::WindingFill); // Roboto Flex's outlines overlap
                glyph.addText(0, 0, bumpFont, ch);
                // squeezed or stretched to its slot as the slot changes width
                // (a squash and stretch in the wave; never into a neighbour)
                const double own = metrics.horizontalAdvance(ch.at(0));
                const double fit = (hasOld && !hasNew) ? 1 : qBound(0.55, own > 0 ? adv / own : 1, 1.6);
                QTransform t;
                t.translate(r.left() + x, baseline - 16 * b);
                t.scale(scale * fit, scale);
                p.fillPath(t.map(glyph), d->scheme.primary);
            }
            x += adv;
        }
        return;
    }
    const qint64 since = d->swapAt >= 0 ? qMax<qint64>(now - d->wordAt, 3000) : now - d->wordAt; // (after the swap: settled)
    if (!d->previousWord.isEmpty() && since < 400) drawWord(d->previousWord, since, true);
    drawWord(d->word, since - 150, false);
}

void OnboardingHello::mousePressEvent(QMouseEvent* event) {
    const qint64 now = d->now();
    const QPointF at = event->position();
    const double h = this->height(), w = this->width();

    // A shape: squash and become the next MaterialShape
    for (int i = slotCount - 1; i >= 0; i--) {
        const Slot& slot = shapeSlots[i];
        const double radius = slot.radius * h;
        if (QLineF(at, QPointF(slot.at.x() * w, slot.at.y() * h)).length() > radius) continue;
        ShapeState& shape = d->shapes[i];
        shape.from = shape.to;
        shape.to = (shape.to + 1) % int(MaterialShapes::all().size());
        shape.morph = std::make_shared<m3shapes::Morph>(MaterialShapes::all()[shape.from].shape(), MaterialShapes::all()[shape.to].shape());
        shape.morphedAt = now;
        shape.poppedAt = now;
        event->accept();
        return;
    }

    // The greeting: the letters jump; five quick clicks and it says so
    if (d->greetingRect.contains(at)) {
        d->jumpAt = now;
        d->recentClicks.append(now);
        while (!d->recentClicks.isEmpty() && now - d->recentClicks.first() > 2000) d->recentClicks.removeFirst();
        if (d->recentClicks.size() >= 5) {
            d->recentClicks.clear();
            d->previousWord = d->word;
            d->word = QStringLiteral("M3 Expressive");
            d->wordAt = now;
            d->expressiveUntil = now + 3200;
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

bool OnboardingHello::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched)
    if (event->type() == QEvent::KeyPress && this->isVisible()) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->isAutoRepeat()) return false;
        if (key->key() == konami.value(d->konamiProgress)) {
            d->konamiProgress++;
            if (d->konamiProgress == konami.size()) {
                d->konamiProgress = 0;
                d->partyUntil = d->now() + 4500;
                d->jumpAt = d->now();
            }
        } else {
            d->konamiProgress = key->key() == konami.first() ? 1 : 0;
        }
    }
    return false;
}

// ---- The "Done" confetti ---------------------------------------------------

namespace {
    class Confetti : public QWidget {
        public:
            Confetti(QWidget* window, QPoint origin) :
                QWidget(window), origin(origin) {
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                this->setGeometry(window->rect());
                scheme = M3::currentScheme();
                const auto& all = MaterialShapes::all();
                for (int i = 0; i < 46; i++) {
                    Piece piece;
                    const double angle = -M_PI / 2 + (QRandomGenerator::global()->generateDouble() - 0.5) * M_PI * 1.3;
                    const double speed = 380 + QRandomGenerator::global()->generateDouble() * 520;
                    piece.velocity = QPointF(std::cos(angle) * speed, std::sin(angle) * speed);
                    piece.spin = (QRandomGenerator::global()->generateDouble() - 0.5) * 720;
                    piece.size = 7 + QRandomGenerator::global()->generateDouble() * 11;
                    piece.shape = QRandomGenerator::global()->bounded(int(all.size()));
                    piece.role = QRandomGenerator::global()->bounded(int(RoleCount));
                    pieces.append(piece);
                }
                clock.start();
                frame.setInterval(16);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    if (clock.elapsed() > 1600) {
                        this->deleteLater();
                        return;
                    }
                    this->update();
                });
                frame.start();
                this->show();
                this->raise();
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const double t = clock.elapsed() / 1000.0;
                const auto& all = MaterialShapes::all();
                for (const Piece& piece : pieces) {
                    // Thrown up and out, falling, slowing in the air, fading at the end
                    const double drag = 1 - std::exp(-2.2 * t);
                    const QPointF at = origin + piece.velocity / 2.2 * drag + QPointF(0, 520 * t * t);
                    const double size = piece.size * bouncy(t * 1.6);
                    QTransform transform;
                    transform.translate(at.x(), at.y());
                    transform.rotate(piece.spin * t);
                    QColor c = roleColour(scheme, piece.role);
                    c.setAlphaF(qBound(0.0, (1.6 - t) / 0.6, 1.0));
                    p.fillPath(transform.map(m3shapes::toPath(all[piece.shape].shape(), QRectF(-size, -size, 2 * size, 2 * size))), c);
                }
            }

        private:
            struct Piece {
                    QPointF velocity;
                    double spin, size;
                    int shape, role;
            };
            QPoint origin;
            QList<Piece> pieces;
            M3::Scheme scheme;
            QElapsedTimer clock;
            QTimer frame;
    };
} // namespace

void OnboardingCelebration::burst(QWidget* window, QPoint origin) {
    new Confetti(window, origin);
}
