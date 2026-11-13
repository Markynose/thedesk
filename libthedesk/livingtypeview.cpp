/****************************************
 *
 *   theDesk - living type loading screen
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "livingtypeview.h"

#include <QEasingCurve>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTimer>
#include <QtMath>
#include "compositor.h"
#include <m3motion.h>
#include <m3scheme.h>
#include <pwd.h>
#include <shapes/m3loadingindicator.h>
#include <tsettings.h>
#include <unistd.h>
#ifdef HAVE_X11
    #include <QDateTime>
    #include <tx11info.h>
    // Xlib last: its macros (None, Bool...) clash with Qt headers
    #include <X11/Xatom.h>
    #include <X11/Xlib.h>
#endif

namespace {
    // ---- Axis quantisation ----------------------------------------------------
    // Glyphs are cached per (char, weight step, width step, size); springs move
    // freely and the drawing snaps to the nearest step
    constexpr double weightSteps[] = {100, 250, 400, 550, 700, 850, 1000};
    constexpr double widthSteps[] = {25, 50, 75, 100, 125, 151};

    template<size_t N> int nearestStep(const double (&steps)[N], double value) {
        int best = 0;
        for (size_t i = 1; i < N; i++) {
            if (std::abs(steps[i] - value) < std::abs(steps[best] - value)) best = int(i);
        }
        return best;
    }

    struct Glyph {
            QPainterPath path; // baseline at y = 0
            double advance = 0;
            QImage image;      // field mode: pre-rendered, origin at imageOrigin
            QPointF imageOrigin;
    };

    class GlyphCache {
        public:
            const Glyph& glyph(QChar ch, int weight, int width, int px) {
                quint64 key = (quint64(ch.unicode()) << 32) | (quint64(weight) << 24) | (quint64(width) << 16) | quint64(px);
                auto it = glyphs.find(key);
                if (it != glyphs.end()) return *it;

                QFont font(QStringLiteral("Roboto Flex"));
                font.setPixelSize(px);
                font.setVariableAxis(QFont::Tag("wght"), float(weightSteps[weight]));
                font.setVariableAxis(QFont::Tag("wdth"), float(widthSteps[width]));
                font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8, px, 144)));
                Glyph g;
                g.path.addText(0, 0, font, QString(ch));
                g.advance = QFontMetricsF(font).horizontalAdvance(ch);
                return *glyphs.insert(key, g);
            }

            // A tinted, pre-rendered copy for drawing many small glyphs fast
            const Glyph& rendered(QChar ch, int weight, int width, int px, QColor colour, qreal dpr) {
                quint64 key = ((quint64(ch.unicode()) << 32) | (quint64(weight) << 24) | (quint64(width) << 16) | quint64(px)) ^ (quint64(colour.rgba()) * 0x9E3779B97F4A7C15ULL);
                auto it = images.find(key);
                if (it != images.end()) return *it;
                Glyph g = glyph(ch, weight, width, px);
                QRectF bounds = g.path.boundingRect().adjusted(-2, -2, 2, 2);
                QImage image((bounds.size() * dpr).toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
                image.setDevicePixelRatio(dpr);
                image.fill(Qt::transparent);
                QPainter p(&image);
                p.setRenderHint(QPainter::Antialiasing);
                p.translate(-bounds.topLeft());
                p.fillPath(g.path, colour);
                p.end();
                g.image = image;
                g.imageOrigin = bounds.topLeft();
                return *images.insert(key, g);
            }

            void clear() {
                glyphs.clear();
                images.clear();
            }

        private:
            QHash<quint64, Glyph> glyphs;
            QHash<quint64, Glyph> images;
    };

    // ---- Springs ----------------------------------------------------------------
    struct Spring {
            double value = 0;
            double velocity = 0;
            double target = 0;

            void set(double v) {
                value = target = v;
                velocity = 0;
            }

            void step(double dt, M3Motion::Token token) {
                if (value == target && velocity == 0) return;
                auto s = M3Motion::stepSpring(value, velocity, target, M3Motion::spring(token), dt);
                value = s.value;
                velocity = s.velocity;
                if (std::abs(value - target) < 1e-3 && std::abs(velocity) < 1e-2) {
                    value = target;
                    velocity = 0;
                }
            }
    };

    // The looks a letter can take
    enum Look {
        Normal,
        Thin,  // "|": narrowest width, heavier, a little taller
        Flat,  // "_": widest, flattened onto the baseline
        Heavy, // heaviest and widest
        Light, // thinnest and narrow
        LookCount
    };

    struct LookTarget {
            double weight, width, scaleX, scaleY;
    };

    LookTarget targetFor(int look) {
        switch (look) {
            case Thin:
                return {700, 25, 0.85, 1.12};
            case Flat:
                return {900, 151, 1.05, 0.2};
            case Heavy:
                return {1000, 151, 1, 1};
            case Light:
                return {100, 60, 1, 1};
            case Normal:
            default:
                return {400, 100, 1, 1};
        }
    }

    struct Letter {
            QChar ch;
            Spring weight, width, scaleX, scaleY, rotation, opacity;
            qint64 applyAt = -1; // pending look change
            int pendingLook = Normal;
            int look = Normal;  // the look it has taken (field mode)
            double offset = 0;  // field mode: x within the word at the normal look
            double exitDelay = 0; // field exits: ms after the exit starts
            double seed = 0;      // field exits: per-letter randomness, 0..1

            void applyLook(int look) {
                LookTarget t = targetFor(look);
                weight.target = t.weight;
                width.target = t.width;
                scaleX.target = t.scaleX;
                scaleY.target = t.scaleY;
            }

            void snapLook(int look) {
                LookTarget t = targetFor(look);
                weight.set(t.weight);
                width.set(t.width);
                scaleX.set(t.scaleX);
                scaleY.set(t.scaleY);
            }

            void step(double dt, M3Motion::Token shape = M3Motion::SpatialDefault) {
                weight.step(dt, shape);
                width.step(dt, shape);
                scaleX.step(dt, shape);
                scaleY.step(dt, shape);
                rotation.step(dt, shape);
                opacity.step(dt, M3Motion::EffectsDefault);
            }
    };

    struct Word {
            QString text;
            QPointF anchor; // 0..1 of the view
            double size;    // fraction of the view height
            int colourRole; // 0 primary, 1 on-surface, 2 tertiary, 3 on-surface-variant
            QList<Letter> letters;
            Spring rotation, opacity, offsetY;
            int look = Normal;
            int orientation = 0; // degrees: 0, 90, -90, 180
            qint64 nextChange = 0;
    };

    // A phrase in the field: changes its look as a whole, sometimes turns on
    // its side or upside down, and now and then becomes a different phrase
    struct FieldWord {
            int phrase = 0;
            int row = 0;
            QList<Letter> letters;
            Spring x; // centre
            double baseline = 0;
            double slotWidth = 0; // room in the row: the widest look
            double textWidth = 0; // width at the normal look
            Spring rotation;
            int look = Normal;
            qint64 nextLook = 0;
            qint64 nextTurn = 0;
            qint64 turnBackAt = -1;
            qint64 nextSwap = 0;
            int swapPhase = 0; // 1: folding away before the new phrase
            qint64 swapAt = -1;
            int nextPhrase = 0;
            // Live footprint, measured every frame from the letters as drawn
            double liveWidth = 0;
            double halfX = 0; // half its extent across, at its current angle
            double halfY = 0; // half its extent up and down
    };

    // How long the hole onto the desktop takes to open past the corners
    constexpr qint64 irisMillis = 700;

    // M3 easing token "emphasized accelerate", for things leaving the screen
    // (https://m3.material.io/styles/motion/easing-and-duration)
    double emphasizedAccelerate(double t) {
        static const QEasingCurve curve = [] {
            QEasingCurve c(QEasingCurve::BezierSpline);
            c.addCubicBezierSegment(QPointF(0.3, 0), QPointF(0.8, 0.15), QPointF(1, 1));
            return c;
        }();
        return curve.valueForProgress(qBound(0.0, t, 1.0));
    }

    QRandomGenerator& rng() {
        static QRandomGenerator generator(QRandomGenerator::global()->generate());
        return generator;
    }

    int randomBetween(int low, int high) {
        return int(rng().bounded(low, high + 1));
    }
} // namespace

struct LivingTypeViewPrivate {
        LivingTypeView::Mode mode = LivingTypeView::Collage;
        double timeScale = 1;
        QTimer frameTimer;
        QElapsedTimer wallClock;
        qint64 lastWall = 0;
        qint64 t = 0; // simulated ms

        M3::Scheme scheme;
        GlyphCache cache;
        QList<Word> words;
        QList<FieldWord> fieldWords;
        QList<double> rowStarts;
        QStringList phrases;
        double separatorWidth = 0;
        int fieldPx = 28;

        double normalAdvance(QChar ch) {
            return cache.glyph(ch, nearestStep(weightSteps, 400), nearestStep(widthSteps, 100), fieldPx).advance;
        }

        double normalWidth(const QString& text) {
            double w = 0;
            for (QChar ch : text) w += normalAdvance(ch);
            return w;
        }

        // Room for a word at its widest look (heavy or flat), so no look
        // ever runs into the next word
        double slotWidth(const QString& text) {
            double w = 0;
            for (QChar ch : text) w += cache.glyph(ch, nearestStep(weightSteps, 1000), nearestStep(widthSteps, 151), fieldPx).advance;
            return w * 1.05;
        }

        QList<Letter> makeLetters(const QString& text, qint64 riseAt, int stagger) {
            QList<Letter> letters;
            double offset = 0;
            int i = 0;
            for (QChar ch : text) {
                Letter letter;
                letter.ch = ch;
                letter.offset = offset;
                letter.snapLook(Flat);
                letter.scaleY.set(0);
                letter.opacity.set(0);
                letter.look = Flat;
                letter.pendingLook = Normal;
                letter.applyAt = riseAt + i * stagger;
                offset += normalAdvance(ch);
                letters.append(letter);
                i++;
            }
            return letters;
        }

        double rowHeight() const {
            return fieldPx * 1.45;
        }

        // Measure a word as it is drawn right now: live letter widths,
        // turned by its current angle
        void measure(FieldWord& word) {
            double w = 0;
            for (const Letter& l : word.letters) {
                w += cache.glyph(l.ch, nearestStep(weightSteps, l.weight.value), nearestStep(widthSteps, l.width.value), fieldPx).advance * qMax(0.0, l.scaleX.value);
            }
            word.liveWidth = w;
            double angle = qDegreesToRadians(word.rotation.value);
            double c = std::abs(std::cos(angle)), s = std::abs(std::sin(angle));
            double h = fieldPx * 0.95;
            word.halfX = (c * w + s * h) / 2;
            word.halfY = (s * w + c * h) / 2;
        }

        // Live layout, every frame: each word takes exactly the room it has
        // now. A word turning on its side narrows in its row and reaches
        // into the rows around it, and they part around it as it turns.
        // Words spring towards their places.
        void layoutLive(bool snap) {
            const double lift = fieldPx * 0.36;
            const double pad = fieldPx * 0.3;
            for (FieldWord& word : fieldWords) measure(word);

            for (int row = 0; row < rowStarts.size(); row++) {
                double bandTop = 0;
                for (const FieldWord& word : fieldWords) {
                    if (word.row == row) {
                        bandTop = word.baseline - lift - rowHeight() / 2;
                        break;
                    }
                }
                double bandBottom = bandTop + rowHeight();

                QList<QPair<double, double>> blocked;
                for (const FieldWord& word : fieldWords) {
                    if (word.row == row || word.halfY <= rowHeight() / 2) continue;
                    double centreY = word.baseline - lift;
                    if (centreY - word.halfY < bandBottom && centreY + word.halfY > bandTop) {
                        blocked.append({word.x.value - word.halfX - pad, word.x.value + word.halfX + pad});
                    }
                }
                std::sort(blocked.begin(), blocked.end());

                double cursor = rowStarts.value(row);
                for (FieldWord& word : fieldWords) {
                    if (word.row != row) continue;
                    double left = cursor;
                    bool moved = true;
                    while (moved) {
                        moved = false;
                        for (const auto& b : blocked) {
                            if (left + 2 * word.halfX > b.first && left < b.second) {
                                left = b.second;
                                moved = true;
                            }
                        }
                    }
                    double centre = left + word.halfX;
                    if (snap) word.x.set(centre);
                    else word.x.target = centre;
                    cursor = left + 2 * word.halfX + separatorWidth;
                }
            }
        }

        int sidewaysCount() const {
            int count = 0;
            for (const FieldWord& word : fieldWords) count += std::abs(std::sin(qDegreesToRadians(word.rotation.target))) > 0.5 ? 1 : 0;
            return count;
        }

        bool flagColours = false;
        bool preview = false;   // clip our own painting instead of masking the window
        bool recovery = false;  // restarting after a crash
        QStringList farewell;   // the goodbye's phrases (shutting down, logging out)
        QPainterPath previewHole; // the exit's hole, in preview or composited
        // With a compositor (picom) and a translucent window, the exit's hole
        // is painted transparent like the preview's: no window reshaping
        // every frame, which X and picom can't keep up with full-screen
        bool composited = false;
        qint64 nextTurnAllowed = 0; // turns are choreographed: one at a time-ish

        int turnedCount() const {
            int count = 0;
            for (const FieldWord& word : fieldWords) count += word.turnBackAt >= 0 ? 1 : 0;
            return count;
        }

        Spring indicatorScale;
        LivingTypeView::ExitStyle exitChoice = LivingTypeView::RandomExit;
        LivingTypeView::ExitStyle exitStyle = LivingTypeView::Cascade; // the one playing
        Spring iris;         // 0..1: the hole onto the desktop
        qint64 irisAt = -1;  // when the hole starts opening
        bool exiting = false;
        qint64 exitStarted = -1;
        qint64 exitDuration = 0;

        // stats (LIVINGTYPE_STATS=1)
        double paintTotal = 0;
        int paintCount = 0;
        qint64 statsAt = 0;
};

LivingTypeView::LivingTypeView(QWidget* parent) :
    QWidget(parent) {
    d = new LivingTypeViewPrivate();
    d->frameTimer.setInterval(16);
    d->frameTimer.setTimerType(Qt::PreciseTimer);
    connect(&d->frameTimer, &QTimer::timeout, this, &LivingTypeView::tick);
    this->setAttribute(Qt::WA_OpaquePaintEvent);
}

LivingTypeView::~LivingTypeView() {
    delete d;
}

QString LivingTypeView::userDisplayName() {
    // Screenshots and recordings for the README use a stand-in name
    if (qEnvironmentVariableIsSet("TD_DISPLAY_NAME")) return qEnvironmentVariable("TD_DISPLAY_NAME");
    struct passwd* pw = getpwuid(getuid());
    if (!pw) return QString::fromLocal8Bit(qgetenv("USER"));
    QString gecos = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
    return gecos.isEmpty() ? QString::fromLocal8Bit(pw->pw_name) : gecos;
}

void LivingTypeView::setMode(Mode mode) {
    d->mode = mode;
    restart();
}

LivingTypeView::Mode LivingTypeView::mode() const {
    return d->mode;
}

void LivingTypeView::setExitStyle(ExitStyle style) {
    d->exitChoice = style;
}

LivingTypeView::ExitStyle LivingTypeView::exitStyle() const {
    return d->exitChoice;
}

QString LivingTypeView::exitStyleName(int style) {
    switch (style) {
        case Vortex:
            return QStringLiteral("vortex");
        case Bloom:
            return QStringLiteral("bloom");
        case Ripple:
            return QStringLiteral("ripple");
        case Drop:
            return QStringLiteral("drop");
        case Sweep:
            return QStringLiteral("sweep");
        case Cascade:
            return QStringLiteral("cascade");
        default:
            return QStringLiteral("random");
    }
}

void LivingTypeView::setRecovery(bool recovery) {
    d->recovery = recovery;
    d->timeScale = recovery ? 0.7 : 1.0; // slower, quieter
    restart();
}

QStringList LivingTypeView::recoveryPhrases() {
    return {
        tr("Picking things back up..."),
        tr("Sorry about that."),
        tr("Back in a moment."),
        tr("Restoring your desktop..."),
        tr("Hang tight."),
        QStringLiteral("TheDesk"),
    };
}

void LivingTypeView::setFarewell(QStringList phrases) {
    d->farewell = phrases;
    restart();
}

void LivingTypeView::setPreview(bool preview) {
    d->preview = preview;
    this->setAttribute(Qt::WA_TranslucentBackground, preview); // the hole shows the parent
}

void LivingTypeView::setFlagColours(bool flag) {
    d->flagColours = flag;
    this->update();
}

QStringList LivingTypeView::phrasesFor(QDate date) {
    QStringList phrases = {
        tr("Loading..."),
        QStringLiteral("TheDesk"),
        tr("Loading in %1...").arg(userDisplayName()),
        tr("Welcome love!"),
    };
    // Splash/seasonal (default on): the awareness months' phrases
    tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
    const bool seasonal = !settings.keysInGroup(QStringLiteral("Splash")).contains(QStringLiteral("seasonal")) || settings.value(QStringLiteral("Splash/seasonal")).toBool();
    if (!seasonal) return phrases;
    if (date.month() == 11) {
        // Movember: men's health, men's mental health and suicide prevention
        phrases << tr("You matter.") << tr("It's okay to talk.") << tr("Check in on your mates.") << tr("Take a breath.") << tr("You're not alone.");
    } else if (date.month() == 5 || (date.month() == 10 && date.day() == 10)) {
        // Mental Health Awareness Month (May), World Mental Health Day (10 October)
        phrases << tr("Be gentle with yourself.") << tr("It's okay to rest.") << tr("You're not alone.") << tr("Take a breath.") << tr("You matter.");
    }
    return phrases;
}

void LivingTypeView::setTimeScale(double scale) {
    d->timeScale = qBound(0.05, scale, 4.0);
}

namespace {
    // The login greeter's exit leaves its loading indicator where this one
    // sits, and its time on the root window: carry on from there (no fresh
    // spring-in) when that was just now. -1 when there's nothing to pick up
    qint64 takeGreeterHandoff() {
#ifdef HAVE_X11
        if (!tX11Info::isPlatformX11()) return -1;
        Display* display = tX11Info::display();
        const Atom atom = XInternAtom(display, "_THEDESK_SPLASH_HANDOFF", True);
        if (atom == None) return -1;
        Atom type;
        int format;
        unsigned long count, after;
        unsigned char* data = nullptr;
        qint64 ms = -1;
        if (XGetWindowProperty(display, tX11Info::appRootWindow(), atom, 0, 2, True, XA_CARDINAL, &type, &format, &count, &after, &data) == Success && data) {
            if (format == 32 && count == 2) {
                auto* values = reinterpret_cast<long*>(data);
                if (QDateTime::currentSecsSinceEpoch() - values[1] < 120) ms = values[0];
            }
            XFree(data);
        }
        return ms;
#else
        return -1;
#endif
    }
} // namespace

void LivingTypeView::restart() {
    d->exiting = false;
    d->exitStarted = -1;
    d->irisAt = -1;
    d->iris.set(0);
    d->previewHole = QPainterPath();
    d->composited = false;
    if (!d->preview && this->window()) this->window()->clearMask();
    d->t = 0;
    build();
    static bool handoffChecked = false; // only the first showing can follow the greeter
    if (!handoffChecked) {
        handoffChecked = true;
        const qint64 greeterMs = takeGreeterHandoff();
        if (greeterMs >= 0) {
            d->indicatorScale.set(1);
            d->t = greeterMs;
        }
    }
    if (this->isVisible()) {
        d->wallClock.start();
        d->lastWall = 0;
        d->frameTimer.start();
    }
    this->update();
}

void LivingTypeView::build() {
    // After a crash the field is calmer: the scheme's neutral variant
    d->scheme = d->recovery ? M3::Scheme::generate(M3::seedFor(M3::Config::fromSettings()), M3::currentScheme().surface.lightnessF() < 0.5, M3::Variant::Neutral)
                            : M3::currentScheme();
    d->cache.clear();
    d->indicatorScale.set(0);
    d->indicatorScale.target = 1;
    const QSizeF size = this->size();
    if (size.isEmpty()) return;

    d->words.clear();
    d->fieldWords.clear();

    const QStringList phrases = d->recovery ? recoveryPhrases() : !d->farewell.isEmpty() ? d->farewell : phrasesFor(QDate::currentDate());

    if (d->mode == Collage) {
        struct Placement {
                int phrase;
                QPointF anchor;
                double size;
                int colour;
        };
        const Placement placements[] = {
            {1, {0.50, 0.25}, 0.16, 0}, // TheDesk
            {0, {0.25, 0.64}, 0.085, 1}, // Loading...
            {3, {0.75, 0.62}, 0.075, 2}, // Welcome love!
            {2, {0.50, 0.86}, 0.05, 3},  // Loading in ...
        };
        int wordIndex = 0;
        for (const Placement& p : placements) {
            Word word;
            word.text = phrases.at(p.phrase);
            word.anchor = p.anchor;
            word.size = p.size;
            word.colourRole = p.colour;
            word.opacity.set(0);
            word.opacity.target = 1;
            word.rotation.set(0);
            word.offsetY.set(size.height() * 0.06);
            word.offsetY.target = 0;
            int index = 0;
            for (QChar ch : word.text) {
                Letter letter;
                letter.ch = ch;
                letter.snapLook(Flat);
                letter.opacity.set(1);
                // Intro: rise out of "_" one letter after another
                letter.pendingLook = Normal;
                letter.applyAt = 250 + wordIndex * 180 + index * 45;
                word.letters.append(letter);
                index++;
            }
            word.nextChange = 1400 + wordIndex * 350 + randomBetween(0, 600);
            d->words.append(word);
            wordIndex++;
        }
    } else {
        // Rows of phrases; each phrase is a word that lives on its own
        // 28 px on a screen; a preview is the screen in miniature
        d->fieldPx = d->preview ? qMax(6, qRound(28 * size.height() / 768.0)) : 28;
        d->phrases = phrases;
        d->fieldWords.clear();
        d->rowStarts.clear();
        d->separatorWidth = d->normalWidth(QStringLiteral("  ·  "));
        const double rowHeight = d->fieldPx * 1.45;
        int row = 0;
        for (double baseline = rowHeight * 0.8; baseline < size.height() + rowHeight; baseline += rowHeight, row++) {
            double start = -40 - (row * 37) % 140;
            d->rowStarts.append(start);
            double x = start;
            int previous = -1;
            int k = 0;
            while (x < size.width() + 60) {
                FieldWord word;
                do {
                    word.phrase = randomBetween(0, int(phrases.size()) - 1);
                } while (word.phrase == previous);
                previous = word.phrase;
                word.row = row;
                word.baseline = baseline;
                QString text = phrases.at(word.phrase);
                word.slotWidth = d->slotWidth(text);
                word.textWidth = d->normalWidth(text);
                // Intro: cascade in from the top
                word.letters = d->makeLetters(text, qint64(baseline / size.height() * 700) + k * 40, 15);
                word.rotation.set(0);
                word.nextLook = randomBetween(1500, 6000);
                word.nextTurn = randomBetween(6000, 28000);
                word.nextSwap = randomBetween(5000, 18000);
                x += word.textWidth + d->separatorWidth;
                d->fieldWords.append(word);
                k++;
            }
        }
        d->layoutLive(true);
    }
}

void LivingTypeView::playExit() {
    if (d->exiting) return;
    d->exiting = true;
    d->composited = !d->preview && this->window() && this->window()->testAttribute(Qt::WA_TranslucentBackground) && Compositor::isActive();
    d->exitStarted = d->t;
    d->indicatorScale.target = 0;
    if (d->mode == Collage) {
        for (int w = 0; w < d->words.size(); w++) {
            Word& word = d->words[w];
            for (int i = 0; i < word.letters.size(); i++) {
                word.letters[i].pendingLook = Flat;
                word.letters[i].applyAt = d->t + w * 80 + i * 25;
            }
        }
        d->exitDuration = 1200;
    } else {
        d->exitStyle = d->exitChoice == RandomExit ? ExitStyle(randomBetween(0, ExitStyleCount - 1)) : d->exitChoice;
        const double h = qMax(1, this->height());
        const double w = qMax(1, this->width());
        const QPointF centre(w / 2, h / 2);
        const double reach = std::hypot(w, h) / 2;
        for (FieldWord& word : d->fieldWords) {
            double centreY = word.baseline - d->fieldPx * 0.36;
            for (Letter& letter : word.letters) {
                letter.seed = rng().generateDouble();
                QPointF at(word.x.value - word.textWidth / 2 + letter.offset, centreY);
                double distance = QLineF(at, centre).length() / reach; // 0..1
                switch (d->exitStyle) {
                    case Cascade:
                        letter.pendingLook = -1; // collapse and fade, top first
                        letter.applyAt = d->t + qint64(word.baseline / h * 650);
                        break;
                    case Ripple:
                        letter.pendingLook = -1; // the ring reaches it
                        letter.applyAt = d->t + 120 + qint64(distance * 1150);
                        break;
                    case Vortex:
                        letter.exitDelay = distance * 420 + letter.seed * 60; // inner first
                        break;
                    case Drop:
                        letter.exitDelay = letter.seed * 420 + (1 - word.baseline / h) * 140;
                        break;
                    case Sweep:
                        letter.pendingLook = Thin; // squeeze to "|" before going
                        letter.applyAt = d->t;
                        letter.exitDelay = 120 + (1 - at.x() / w) * 380 + letter.seed * 40;
                        break;
                    case Bloom:
                        letter.exitDelay = distance * 260;
                        break;
                    default:
                        break;
                }
            }
        }
        // Every exit ends the same way: once the words are gone the indicator
        // swells, then opens as a morphing M3E shape onto the desktop
        qint64 wordsGone = 700;
        double swell = 2.2;
        switch (d->exitStyle) {
            case Vortex:
                wordsGone = 980; // swallows the words first
                swell = 1.7;
                break;
            case Bloom:
                wordsGone = 420;
                swell = 3.2;
                break;
            case Ripple:
                wordsGone = 1150;
                break;
            case Drop:
                wordsGone = 850;
                break;
            case Sweep:
                wordsGone = 760;
                break;
            case Cascade:
            default:
                wordsGone = 700;
                break;
        }
        d->indicatorScale.target = swell;
        d->irisAt = d->t + wordsGone;
        d->exitDuration = wordsGone + irisMillis + 40;
    }
}

void LivingTypeView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    restart();
}

void LivingTypeView::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->frameTimer.stop();
}

void LivingTypeView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    build();
}

void LivingTypeView::tick() {
    qint64 wall = d->wallClock.elapsed();
    double dtMs = qMin<qint64>(wall - d->lastWall, 50) * d->timeScale; // a stalled frame doesn't jump
    d->lastWall = wall;
    d->t += qint64(dtMs);
    const double dt = dtMs / 1000.0;
    d->indicatorScale.step(dt, M3Motion::SpatialDefault);

    if (d->mode == Collage) {
        for (Word& word : d->words) {
            // Time for a new pose: a look, and maybe a turn
            if (!d->exiting && d->t >= word.nextChange) {
                int look;
                do {
                    look = randomBetween(0, LookCount - 1);
                } while (look == word.look);
                word.look = look;

                double longSide = word.size * this->height() * 0.62 * word.text.length();
                bool canTurn = longSide < this->height() * 0.42;
                int roll = randomBetween(0, 99);
                int orientation = roll < 55 ? 0 : roll < 80 ? (canTurn ? (randomBetween(0, 1) ? 90 : -90) : 0)
                                                            : 180;
                // Turn the short way round to the new orientation
                double current = word.rotation.target;
                double delta = std::remainder(orientation - current, 360.0);
                word.rotation.target = current + delta;
                word.orientation = orientation;

                for (int i = 0; i < word.letters.size(); i++) {
                    word.letters[i].pendingLook = look;
                    word.letters[i].applyAt = d->t + i * 45;
                }
                word.nextChange = d->t + randomBetween(1800, 3200);
            }
            if (d->exiting && d->t - d->exitStarted > 250) {
                word.offsetY.target = this->height() * 0.7;
                word.opacity.target = 0;
            }

            for (int i = 0; i < word.letters.size(); i++) {
                Letter& letter = word.letters[i];
                if (letter.applyAt >= 0 && d->t >= letter.applyAt) {
                    letter.applyLook(letter.pendingLook);
                    letter.applyAt = -1;
                }
                // Normal letters breathe a little in weight
                if (!d->exiting && word.look == Normal && letter.applyAt < 0) {
                    letter.weight.target = 400 + 90 * std::sin(d->t / 650.0 + i * 0.7);
                }
                letter.step(dt);
            }
            word.rotation.step(dt, M3Motion::SpatialDefault);
            word.opacity.step(dt, M3Motion::EffectsDefault);
            word.offsetY.step(dt, M3Motion::SpatialSlow);
        }
    } else {
        const QPointF centre(this->width() / 2.0, this->height() / 2.0);
        const double clearRadius = 150;
        for (FieldWord& word : d->fieldWords) {
            if (!d->exiting) {
                // A new look for the whole word, letter after letter
                if (word.swapPhase == 0 && d->t >= word.nextLook) {
                    int look;
                    do {
                        look = randomBetween(0, 7);
                    } while (look >= LookCount ? false : look == word.look);
                    if (look >= LookCount) look = Normal; // normal is the most common
                    word.look = look;
                    for (int i = 0; i < word.letters.size(); i++) {
                        word.letters[i].pendingLook = look;
                        word.letters[i].applyAt = d->t + i * 35;
                    }
                    word.nextLook = d->t + randomBetween(2200, 5500);
                }

                // Now and then the whole word turns on its side or upside down;
                // on its side it becomes a column and the rows make room
                if (word.turnBackAt >= 0 && d->t >= word.turnBackAt) {
                    word.rotation.target += std::remainder(-word.rotation.target, 360.0);
                    word.turnBackAt = -1;
                } else if (word.turnBackAt < 0 && word.swapPhase == 0 && d->t >= word.nextTurn && (d->t < d->nextTurnAllowed || d->turnedCount() >= 3)) {
                    // Someone else is turning: wait for a calmer moment
                    word.nextTurn = d->t + randomBetween(3000, 9000);
                } else if (word.turnBackAt < 0 && word.swapPhase == 0 && d->t >= word.nextTurn) {
                    d->nextTurnAllowed = d->t + 1500;
                    // Sideways only where there's room for the column: not
                    // off the edges, not over the indicator, three at most
                    double centreY = word.baseline - d->fieldPx * 0.36;
                    bool fits = centreY - word.liveWidth / 2 > 4 && centreY + word.liveWidth / 2 < this->height() - 4 && word.x.value > 40 && word.x.value < this->width() - 40 && QLineF(QPointF(word.x.value, centreY), centre).length() > clearRadius + word.liveWidth / 2 + 40 && d->sidewaysCount() < 2;
                    int r = randomBetween(0, 2);
                    double orientation = (r < 2 && fits) ? (r == 0 ? 90 : -90) : 180;
                    word.rotation.target += std::remainder(orientation - word.rotation.target, 360.0);
                    word.turnBackAt = d->t + randomBetween(3500, 6000);
                    word.nextTurn = d->t + randomBetween(14000, 32000);
                }

                // Now and then the word becomes another phrase: fold to "_",
                // swap, rise; the row makes room
                if (word.swapPhase == 0 && d->t >= word.nextSwap) {
                    word.swapPhase = 1;
                    do {
                        word.nextPhrase = randomBetween(0, int(d->phrases.size()) - 1);
                    } while (word.nextPhrase == word.phrase);
                    for (int i = 0; i < word.letters.size(); i++) {
                        word.letters[i].pendingLook = -1;
                        word.letters[i].applyAt = d->t + i * 20;
                    }
                    word.swapAt = d->t + 380 + word.letters.size() * 20;
                } else if (word.swapPhase == 1 && d->t >= word.swapAt) {
                    word.phrase = word.nextPhrase;
                    QString text = d->phrases.at(word.phrase);
                    word.letters = d->makeLetters(text, d->t, 30);
                    word.look = Normal;
                    word.slotWidth = d->slotWidth(text);
                    word.textWidth = d->normalWidth(text);
                    word.swapPhase = 0;
                    word.nextSwap = d->t + randomBetween(6000, 18000);
                    word.nextLook = d->t + randomBetween(2000, 4000);
                }
            }
            word.x.step(dt, M3Motion::SpatialDefault);
            word.rotation.step(dt, M3Motion::SpatialSlow);

            // The wave: weight breathing for words at their normal look
            double wave = std::sin(2 * M_PI * ((word.x.value * 0.9 + word.baseline * 1.3) / 1100.0 - d->t / 5200.0));
            for (int i = 0; i < word.letters.size(); i++) {
                Letter& letter = word.letters[i];
                if (letter.applyAt >= 0 && d->t >= letter.applyAt) {
                    letter.look = letter.pendingLook;
                    letter.applyAt = -1;
                }
                if (letter.look < 0) {
                    letter.applyLook(Flat);
                    letter.scaleY.target = 0;
                    letter.opacity.target = 0;
                } else {
                    letter.applyLook(letter.look);
                    letter.opacity.target = 1;
                    if (letter.look == Normal) letter.weight.target = 400 + 140 * wave;

                    // Make room for the indicator: collapse to "_" and fade
                    if (!d->exiting) {
                        QPointF at(word.x.value - word.textWidth / 2 + letter.offset, word.baseline);
                        double room = qBound(0.0, (clearRadius + 90 - QLineF(at, centre).length()) / 90, 1.0);
                        if (room > 0) {
                            letter.applyLook(Flat);
                            letter.scaleY.target = 0.2 * (1 - room);
                            letter.opacity.target = 1 - room;
                        }
                    }
                }
                letter.step(dt);
            }
        }
        if (!d->exiting) d->layoutLive(false);
    }

    // The hole onto the desktop: the window keeps everything outside a
    // morphing M3E shape that grows from the indicator (a window mask, so it
    // works without a compositor)
    const bool irisOpening = d->irisAt >= 0 && d->t >= d->irisAt;
    if (irisOpening) {
        // It starts as the swelled indicator and accelerates until even the
        // shape's dips (about half its radius; the indicator fills 38/48 of
        // its box) are past the corners, so the screen zooms away completely
        const double progress = double(d->t - d->irisAt) / irisMillis;
        d->iris.set(qBound(0.0, progress, 1.0));
        const double start = 72 * qMax(0.0, d->indicatorScale.value) * (d->preview ? this->height() / 768.0 : 1.0);
        const double reach = std::hypot(this->width(), this->height()) * 2.6;
        double side = qMax(1.0, start + (reach - start) * emphasizedAccelerate(progress));
        QRectF box(this->width() / 2.0 - side / 2, this->height() / 2.0 - side / 2, side, side);
        QPainterPath hole = M3LoadingIndicator::indeterminatePath(box, d->t);
        if (d->preview || d->composited) {
            d->previewHole = hole;
        } else {
            QRegion mask = QRegion(this->rect()).subtracted(QRegion(hole.toFillPolygon().toPolygon()));
            // Once the hole covers everything the mask is empty, and an empty mask
            // means "no mask" to Qt: the whole window would show again (a blink).
            // Keep a single pixel just outside the window instead
            if (mask.isEmpty()) this->window()->setMask(QRegion(-2, -2, 1, 1));
            else this->window()->setMask(mask.translated(this->mapTo(this->window(), QPoint(0, 0))));
        }
    }

    if (d->exiting && d->t - d->exitStarted >= d->exitDuration) {
        d->frameTimer.stop();
        emit exitFinished();
    }
    // While the hole opens, what's left of this screen is plain surface (the
    // words are gone and the indicator is inside the hole): no need to
    // repaint it, which kept the last frames from keeping up
    if (!irisOpening || d->preview || d->composited) this->update();
}

void LivingTypeView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QElapsedTimer paintTimer;
    paintTimer.start();

    QPainter painter(this);
    if ((d->preview || d->composited) && !d->previewHole.isEmpty()) {
        // The exit's hole shows what's behind this view
        QPainterPath kept;
        kept.addRect(QRectF(this->rect()));
        painter.setClipPath(kept.subtracted(d->previewHole));
    }
    painter.fillRect(this->rect(), d->scheme.surface);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal dpr = this->devicePixelRatioF();

    auto colourFor = [this](int role) {
        switch (role) {
            case 0:
                return d->scheme.primary;
            case 2:
                return d->scheme.tertiary;
            case 3:
                return d->scheme.onSurfaceVariant;
            default:
                return d->scheme.onSurface;
        }
    };

    if (d->mode == Collage) {
        for (const Word& word : d->words) {
            const int px = qMax(12, int(word.size * this->height()));
            // Lay the letters out on a baseline with their current advances
            QList<double> xs;
            double x = 0;
            for (const Letter& letter : word.letters) {
                const Glyph& g = d->cache.glyph(letter.ch, nearestStep(weightSteps, letter.weight.value), nearestStep(widthSteps, letter.width.value), px);
                xs.append(x);
                x += g.advance * letter.scaleX.value;
            }
            const double wordWidth = x;
            QColor colour = colourFor(word.colourRole);

            painter.save();
            painter.translate(word.anchor.x() * this->width(), word.anchor.y() * this->height() + word.offsetY.value);
            painter.rotate(word.rotation.value);
            painter.translate(-wordWidth / 2, px * 0.36); // centre on the x-height
            painter.setOpacity(qBound(0.0, word.opacity.value, 1.0));
            for (int i = 0; i < word.letters.size(); i++) {
                const Letter& letter = word.letters[i];
                const Glyph& g = d->cache.glyph(letter.ch, nearestStep(weightSteps, letter.weight.value), nearestStep(widthSteps, letter.width.value), px);
                painter.save();
                painter.translate(xs[i], 0);
                painter.scale(qMax(0.0, letter.scaleX.value), qMax(0.0, letter.scaleY.value)); // about the baseline
                painter.fillPath(g.path, colour);
                painter.restore();
            }
            painter.restore();
        }
    } else {
        const QColor base = d->scheme.onSurfaceVariant;
        const QColor crest = d->scheme.primary;
        const double lift = d->fieldPx * 0.36; // turn about the x-height, not the baseline
        for (const FieldWord& word : d->fieldWords) {
            if (word.x.value + word.halfX < -80 || word.x.value - word.halfX > this->width() + 80) continue;
            double wave = std::sin(2 * M_PI * ((word.x.value * 0.9 + word.baseline * 1.3) / 1100.0 - d->t / 5200.0));
            bool atCrest = wave > 0.6;
            QColor colour = atCrest ? crest : base;
            double alpha = atCrest ? 0.42 : 0.2;
            if (d->flagColours) {
                // MLM flag (2019), top to bottom: the rows become its stripes
                static const QColor stripes[] = {QColor(0x078d70), QColor(0x26ceaa), QColor(0x98e8c1), QColor(0xffffff), QColor(0x7bade2), QColor(0x5049cc), QColor(0x3d1a78)};
                int stripe = qBound(0, int(word.row * 7 / qMax(1, int(d->rowStarts.size()))), 6);
                colour = stripes[stripe];
                alpha = atCrest ? 0.9 : 0.55;
            }

            // Live widths: letters change their advance as they change look
            double total = 0;
            QList<double> advances;
            for (const Letter& letter : word.letters) {
                double a = d->cache.glyph(letter.ch, nearestStep(weightSteps, letter.weight.value), nearestStep(widthSteps, letter.width.value), d->fieldPx).advance * qMax(0.0, letter.scaleX.value);
                advances.append(a);
                total += a;
            }

            painter.save();
            painter.translate(word.x.value, word.baseline - lift);
            if (word.rotation.value != 0) painter.rotate(word.rotation.value);
            painter.translate(-total / 2, lift);
            double x = 0;
            for (int i = 0; i < word.letters.size(); i++) {
                const Letter& letter = word.letters[i];
                double opacity = qBound(0.0, letter.opacity.value, 1.0);
                if (!letter.ch.isSpace() && opacity > 0.02 && letter.scaleY.value > 0.01) {
                    const Glyph& g = d->cache.rendered(letter.ch, nearestStep(weightSteps, letter.weight.value), nearestStep(widthSteps, letter.width.value), d->fieldPx, colour, dpr);
                    // Exits that move letters: where this one is on screen,
                    // and where the exit takes it
                    double exitProgress = 0;
                    const bool moving = d->exiting && (d->exitStyle == Vortex || d->exitStyle == Drop || d->exitStyle == Sweep || d->exitStyle == Bloom);
                    if (moving) {
                        const double durations[] = {560, 300, 0, 650, 520, 0};
                        double elapsed = d->t - d->exitStarted - letter.exitDelay;
                        exitProgress = elapsed > 0 ? emphasizedAccelerate(elapsed / durations[d->exitStyle]) : 0;
                    }
                    painter.save();
                    if (exitProgress > 0) {
                        const QPointF centre(this->width() / 2.0, this->height() / 2.0);
                        QPointF at = painter.worldTransform().map(QPointF(x + advances[i] / 2, -lift));
                        double spin = 0, scale = 1, fade = 1;
                        QPointF to = at;
                        switch (d->exitStyle) {
                            case Vortex: {
                                // Spiral in: further out turns more, everything shrinks into the centre
                                double distance = QLineF(at, centre).length() / (std::hypot(this->width(), this->height()) / 2);
                                spin = exitProgress * (200 + 280 * (1 - distance));
                                QTransform swirl;
                                swirl.rotate(spin);
                                to = centre + swirl.map(at - centre) * (1 - exitProgress);
                                scale = 1 - 0.85 * exitProgress;
                                fade = 1 - std::pow(exitProgress, 3);
                                break;
                            }
                            case Drop:
                                to = at + QPointF(0, exitProgress * (this->height() - at.y() + 120));
                                spin = (letter.seed - 0.5) * 70 * exitProgress;
                                break;
                            case Sweep:
                                to = at + QPointF(exitProgress * (this->width() - at.x() + 240), 0);
                                fade = 1 - 0.3 * exitProgress;
                                break;
                            case Bloom:
                                to = at + (at - centre) * 0.15 * exitProgress;
                                scale = 1 - 0.4 * exitProgress;
                                fade = 1 - exitProgress;
                                break;
                            default:
                                break;
                        }
                        painter.resetTransform();
                        painter.translate(to);
                        painter.rotate(word.rotation.value + spin);
                        painter.scale(scale, scale);
                        painter.translate(-advances[i] / 2, lift);
                        opacity *= fade;
                    } else {
                        painter.translate(x, 0);
                    }
                    painter.setOpacity(opacity * alpha);
                    painter.scale(qMax(0.0, letter.scaleX.value), qMax(0.0, letter.scaleY.value));
                    painter.drawImage(g.imageOrigin, g.image);
                    painter.restore();
                }
                x += advances[i];
            }
            painter.restore();

            // A faint dot between this word and the next (gone once it exits)
            painter.setOpacity(d->exiting ? 0.18 * qBound(0.0, 1 - (d->t - d->exitStarted) / 300.0, 1.0) : 0.18);
            painter.setPen(Qt::NoPen);
            painter.setBrush(base);
            painter.drawEllipse(QPointF(word.x.value + word.halfX + d->separatorWidth / 2, word.baseline - lift), 1.6, 1.6);
        }
        painter.setOpacity(1);
    }

    // Ripple exit: the ring that flattens the words as it passes
    if (d->exiting && d->exitStyle == Ripple && d->mode == Field) {
        double elapsed = d->t - d->exitStarted - 120;
        if (elapsed > 0) {
            double reach = std::hypot(this->width(), this->height()) / 2;
            double radius = elapsed / 1150.0 * reach;
            double fade = qBound(0.0, 1 - elapsed / 1400.0, 1.0);
            QColor ring = d->scheme.primary;
            ring.setAlphaF(0.55 * fade);
            painter.setPen(QPen(ring, 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QPointF(this->width() / 2.0, this->height() / 2.0), radius, radius);
            ring.setAlphaF(0.12 * fade);
            painter.setPen(QPen(ring, 18));
            painter.drawEllipse(QPointF(this->width() / 2.0, this->height() / 2.0), radius - 10, radius - 10);
        }
    }

    // The M3 Expressive loading indicator in the middle
    double scale = qMax(0.0, d->indicatorScale.value);
    if (scale > 0.01) {
        double side = 72 * scale * (d->preview ? this->height() / 768.0 : 1.0);
        QRectF box(this->width() / 2.0 - side / 2, this->height() / 2.0 - side / 2, side, side);
        M3LoadingIndicator::paint(&painter, box, qint64(d->t), d->scheme.primary, QColor());
    }

    if (d->composited && !d->previewHole.isEmpty()) {
        // What's under this view in the window (the page's fill) clears too:
        // the hole shows the desktop, the compositor blends it
        painter.setClipping(false);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillPath(d->previewHole, Qt::transparent);
    }

    if (qEnvironmentVariableIsSet("LIVINGTYPE_STATS")) {
        d->paintTotal += paintTimer.nsecsElapsed() / 1e6;
        d->paintCount++;
        if (d->wallClock.elapsed() - d->statsAt > 2000) {
            qInfo("living type: %.1f ms per frame over %d frames (%s)", d->paintTotal / qMax(1, d->paintCount), d->paintCount, d->mode == Collage ? "collage" : "field");
            d->paintTotal = 0;
            d->paintCount = 0;
            d->statsAt = d->wallClock.elapsed();
        }
    }
}
