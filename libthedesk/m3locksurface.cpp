/****************************************
 *
 *   theDesk - the lock and login surface
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3locksurface.h"

#include "horizon/horizonscene.h"
#include "horizon/horizonshapes.h"
#include "horizon/horizonview.h"
#include "m3expressiveclock.h"
#include "m3usercard.h"
#include <QDate>
#include <QEasingCurve>
#include <QLineF>
#include <QRegion>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QtMath>
#include <functional>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/m3loadingindicator.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <tx11info.h>

#ifdef HAVE_X11
    #include <X11/Xatom.h>
    #include <X11/Xlib.h>
#endif

namespace {
    // What Plymouth (and Limine) were drawn in: tools/boot-theme bakes them
    // from the aurora-night scene
    M3::Scheme bootScheme() {
        return M3::Scheme::generate(QColor(0x203626), true, M3::Variant::Vibrant);
    }

    QColor bootSky() {
        return HorizonShapes::sky(HorizonScene::AuroraNight);
    }

    double ease(double x) {
        x = qBound(0.0, x, 1.0);
        return 1 - std::pow(1 - x, 3);
    }

    // Step response of an M3 spring, from 0 at t <= 0 (seconds)
    double spring(double t, M3Motion::Token token) {
        if (t <= 0) return 0;
        double z = 0.8, k = 380;
        if (token == M3Motion::SpatialFast) {
            z = 0.6;
            k = 800;
        } else if (token == M3Motion::SpatialSlow) {
            z = 0.8;
            k = 200;
        }
        const double w = std::sqrt(k);
        const double wd = w * std::sqrt(1 - z * z);
        return 1 - std::exp(-z * w * t) * (std::cos(wd * t) + z * w / wd * std::sin(wd * t));
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    }

    QRectF lerpRect(const QRectF& a, const QRectF& b, double t) {
        return QRectF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t, a.width() + (b.width() - a.width()) * t, a.height() + (b.height() - a.height()) * t);
    }

    QPainterPath glyph(const QString& text, double px, double wght, double wdth) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qMax(1, int(px)));
        font.setVariableAxis(QFont::Tag("opsz"), float(qMin(px, 144.0)));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        path.addText(0, 0, font, text);
        return path;
    }

    // The M3E wavy progress bar, as Plymouth draws it (a 240 px bar, 3 px
    // waves 40 px long drifting back toward the start); `flat` irons it out
    void paintWavyBar(QPainter& p, const QRectF& bar, double progress, double seconds, double flat, const M3::Scheme& s) {
        const double amplitude = 3 * (1 - flat), wavelength = 40, stroke = 4, gap = 6;
        const double y = bar.center().y(), end = bar.left() + bar.width() * progress;
        QPainterPath wave;
        const double offset = std::fmod(seconds * 36, wavelength);
        for (double x = bar.left(); x <= end; x += 1) {
            const double yy = y + amplitude * std::sin(2 * M_PI * (x - bar.left() + offset) / wavelength);
            if (x == bar.left()) wave.moveTo(x, yy);
            else wave.lineTo(x, yy);
        }
        p.setPen(QPen(s.primary, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (progress > 0.01) p.drawPath(wave);
        if (end + gap + stroke < bar.right()) {
            p.setPen(QPen(s.secondaryContainer, stroke, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(end + gap + stroke, y), QPointF(bar.right(), y));
        }
    }

    // Draws the hand-overs on top of the surface: the boot opening and the
    // exit into the loading screen
    class Choreography : public QWidget {
        public:
            enum Mode {
                Idle,
                Boot,
                Exit
            };

            explicit Choreography(QWidget* parent) :
                QWidget(parent) {
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                frame.setInterval(16);
                frame.setTimerType(Qt::PreciseTimer);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    this->tick();
                });
            }

            Mode mode = Idle;
            QElapsedTimer clock;
            QTimer frame;
            QRectF avatar, field;
            std::unique_ptr<m3shapes::Morph> markMorph; // the mark into the picture's shape
            QImage picture;
            QString initial;
            M3::Scheme live;
            std::function<void()> bootDone, exitDone;
            bool doneSent = false;

            static constexpr double bootLength = 1.5, exitLength = 1.1;

            void start(Mode m) {
                mode = m;
                doneSent = false;
                live = M3::currentScheme();
                clock.start();
                frame.start();
                this->show();
                this->raise();
                this->update();
            }

            double seconds() const {
                return clock.isValid() ? clock.elapsed() / 1000.0 : 0;
            }

            void tick() {
                const double t = seconds();
                if (mode == Boot && t >= bootLength && !doneSent) {
                    doneSent = true;
                    frame.stop();
                    mode = Idle;
                    this->hide();
                    if (bootDone) bootDone();
                    return;
                }
                if (mode == Exit && t >= exitLength && !doneSent) {
                    doneSent = true;
                    if (exitDone) exitDone(); // keeps running: the indicator turns until the session takes over
                }
                this->update();
            }

        protected:
            void paintEvent(QPaintEvent* event) override {
                Q_UNUSED(event)
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                if (mode == Boot) paintBoot(p);
                else if (mode == Exit) paintExit(p);
            }

        private:
            void paintBoot(QPainter& p) {
                const double t = seconds();
                const M3::Scheme boot = bootScheme();
                const QPointF centre(this->width() / 2.0, this->height() / 2.0);
                const QRectF mark(centre.x() - 64, centre.y() - 96, 128, 128);
                const QRectF bar(centre.x() - 120, centre.y() + 76, 240, 8);
                const double colour = ease((t - 0.3) / 1.0);

                // Plymouth's sky, giving way to the rising horizon underneath
                QColor sky = bootSky();
                sky.setAlphaF(1 - ease((t - 0.35) / 0.9));
                p.fillRect(this->rect(), sky);

                // The bar completes and irons flat, then stretches into the field
                const double u = t - 0.45;
                if (u <= 0) {
                    paintWavyBar(p, bar, 0.8 + 0.2 * ease(t / 0.4), t, ease(t / 0.45), boot);
                } else {
                    const double k = qMin(spring(u, M3Motion::SpatialDefault), 1.05);
                    QRectF pill = lerpRect(QRectF(bar.left(), bar.center().y() - 2, bar.width(), 4), field, k);
                    QPainterPath path;
                    path.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
                    QColor fill = mix(boot.primary, live.surfaceContainerHigh, ease(u / 0.35));
                    fill.setAlphaF(1 - ease((t - (bootLength - 0.25)) / 0.25));
                    p.fillPath(path, fill);
                }

                // The mark: its shape morphs into the picture's, the d squeezes away
                const double k = qBound(0.0, spring(u, M3Motion::SpatialDefault), 1.08);
                const QRectF from = mark.adjusted(-14, -14, 14, 14);
                const QRectF to = avatar.isValid() ? avatar : from;
                const QRectF r = lerpRect(from, to, k);
                if (!markMorph) markMorph = std::make_unique<m3shapes::Morph>(MaterialShapes::sunny(), MaterialShapes::cookie4Sided());
                QPainterPath shape = m3shapes::toPath(*markMorph, float(qBound(0.0, k, 1.0)), r);
                QTransform spin;
                spin.translate(r.center().x(), r.center().y());
                spin.rotate(45 * (1 - qBound(0.0, k, 1.0)));
                spin.translate(-r.center().x(), -r.center().y());
                shape = spin.map(shape);
                const double handOver = 1 - ease((t - (bootLength - 0.2)) / 0.2);
                p.setOpacity(handOver);
                p.fillPath(shape, mix(boot.tertiaryContainer, live.primaryContainer, colour));
                const double reveal = ease((u - 0.3) / 0.5);
                if (reveal > 0) {
                    p.save();
                    p.setClipPath(shape);
                    p.setOpacity(handOver * reveal);
                    if (!picture.isNull()) {
                        p.drawImage(r, picture.scaled(r.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                    } else {
                        QFont font(QStringLiteral("Roboto Flex"));
                        font.setPixelSize(qMax(1, int(r.height() * 0.45)));
                        font.setVariableAxis(QFont::Tag("wght"), 900);
                        p.setFont(font);
                        p.setPen(live.onPrimaryContainer);
                        p.drawText(r, Qt::AlignCenter, initial);
                    }
                    p.restore();
                }
                const double squeeze = qBound(0.0, u / 0.45, 1.0);
                if (squeeze < 1) {
                    p.setOpacity(handOver * (1 - squeeze));
                    QPainterPath d = glyph(QStringLiteral("d"), mark.height() * 0.48, 1000, 151 - 126 * ease(squeeze));
                    p.fillPath(d.translated(r.center() - d.boundingRect().center()), mix(boot.onTertiaryContainer, live.onPrimaryContainer, colour));
                }
                p.setOpacity(1);
            }

            void paintExit(QPainter& p) {
                const double e = seconds();
                const QPointF centre(this->width() / 2.0, this->height() / 2.0);

                // Everything else gives way to the loading screen's surface
                QColor surface = live.surface;
                surface.setAlphaF(ease((e - 0.15) / 0.8));
                p.fillRect(this->rect(), surface);

                // The field: a circle, then the indicator where the loading screen has it
                const double k1 = spring(e, M3Motion::SpatialFast);
                const double k2 = qMin(spring(e - 0.25, M3Motion::SpatialDefault), 1.05);
                const QRectF circle(field.center().x() - field.height() / 2, field.top(), field.height(), field.height());
                const QRectF box(centre.x() - 36, centre.y() - 36, 72, 72);
                const QRectF r = lerpRect(lerpRect(field, circle, qMin(k1, 1.0)), box, k2);
                QPainterPath pill;
                pill.addRoundedRect(r, r.height() / 2, r.height() / 2);
                QColor fill = live.surfaceContainerHigh;
                fill.setAlphaF(1 - ease((e - 0.2) / 0.5));
                p.fillPath(pill, fill);
                p.setOpacity(ease((e - 0.12) / 0.3));
                M3LoadingIndicator::paint(&p, r, qint64(e * 1000), live.primary, QColor());
                p.setOpacity(1);
            }
    };
} // namespace

struct M3LockSurfacePrivate {
        HorizonView* horizon;
        Choreography* choreography;
        M3ExpressiveClock* clock;
        QLabel* date;
        QWidget* note = nullptr;
        M3UserCard* card;
        QLineEdit* field;
        QToolButton* go;
        QLabel* message;
        QWidget* actions;
        QHBoxLayout* actionsLayout;
        QString picturePath;
        QString displayName;
        QVariantAnimation* shake;
        bool exiting = false;
        int clockPx = 120;
        // Unlocking: a hole grows from the field onto the desktop
        QTimer* irisTimer = nullptr;
        QElapsedTimer irisClock;
};

M3LockSurface::M3LockSurface(QWidget* parent) :
    QWidget(parent) {
    d = new M3LockSurfacePrivate();

    d->horizon = new HorizonView(this);
    d->horizon->setTopShade(true);

    d->clock = new M3ExpressiveClock(this);
    d->clock->setStyle(M3ExpressiveClock::Style::Stack);
    d->clock->setPixelSize(d->clockPx);
    d->date = new QLabel(this);
    {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(20);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("wdth"), 112);
        d->date->setFont(font);
        d->date->setProperty("m3NoHeader", true);
    }
    auto updateDate = [this] {
        d->date->setText(QLocale().toString(QDate::currentDate(), QStringLiteral("dddd, d MMMM")));
    };
    updateDate();
    auto* dateTimer = new QTimer(this);
    dateTimer->setInterval(60000);
    connect(dateTimer, &QTimer::timeout, this, updateDate);
    dateTimer->start();

    d->card = new M3UserCard(this);
    d->card->setCardSize(M3UserCard::Large);
    d->card->setAlignment(Qt::AlignHCenter);

    // The password field: a 56 dp pill with a filled "go" icon button inside
    d->field = new QLineEdit(this);
    d->field->setProperty("m3SearchBar", true);
    d->field->setEchoMode(QLineEdit::Password);
    d->field->setPlaceholderText(tr("Password"));
    d->field->setTextMargins(8, 0, 52, 0);
    d->field->installEventFilter(this);
    d->go = new QToolButton(d->field);
    d->go->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    d->go->setProperty("m3IconButtonStyle", QStringLiteral("filled"));
    d->go->setIconSize(QSize(24, 24));
    d->go->setFixedSize(40, 40);
    d->go->setCursor(Qt::PointingHandCursor);
    d->go->setToolTip(tr("Unlock"));
    connect(d->go, &QToolButton::clicked, this, [this] {
        if (!d->exiting && d->field->isEnabled()) emit submitted(d->field->text());
    });
    connect(d->field, &QLineEdit::returnPressed, d->go, &QToolButton::click);

    d->message = new QLabel(this);
    d->message->setAlignment(Qt::AlignHCenter);
    d->message->setWordWrap(true);
    d->message->setProperty("m3NoHeader", true);

    d->actions = new QWidget(this);
    d->actionsLayout = new QHBoxLayout(d->actions);
    d->actionsLayout->setContentsMargins(0, 0, 0, 0);
    d->actionsLayout->setSpacing(8);

    // A wrong password: the field shakes (a damped spring, sideways)
    d->shake = new QVariantAnimation(this);
    d->shake->setStartValue(0.0);
    d->shake->setEndValue(1.0);
    d->shake->setDuration(450);
    connect(d->shake, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const double t = value.toDouble();
        const double offset = 14 * std::exp(-5 * t) * std::sin(t * 6 * M_PI);
        this->relayout();
        d->field->move(d->field->x() + int(offset), d->field->y());
    });

    d->choreography = new Choreography(this);
    d->choreography->hide();
    d->choreography->bootDone = [this] {
        for (QWidget* w : {static_cast<QWidget*>(d->clock), static_cast<QWidget*>(d->date), static_cast<QWidget*>(d->card), static_cast<QWidget*>(d->field), static_cast<QWidget*>(d->actions)}) w->show();
        d->card->playEntrance();
        this->focusPassword();
    };
    d->choreography->exitDone = [this] {
        emit exitFinished();
    };

    this->setUser(QString());
}

M3LockSurface::~M3LockSurface() {
    delete d;
}

HorizonView* M3LockSurface::horizon() const {
    return d->horizon;
}

void M3LockSurface::setUser(const QString& displayName, const QString& picturePath) {
    d->displayName = displayName;
    d->picturePath = picturePath.isEmpty() && displayName.isEmpty() ? M3UserCard::currentUserPicture() : picturePath;
    d->card->setUser(displayName, picturePath);
    this->relayout();
}

void M3LockSurface::setGreeting(const QString& greeting) {
    // Empty: no greeting line, just the picture and the name
    d->card->setGreetingShown(!greeting.isEmpty());
    d->card->setGreeting(greeting);
}

void M3LockSurface::addAction(QWidget* action) {
    d->actionsLayout->insertWidget(0, action);
    this->relayout();
}

void M3LockSurface::setPlaceholder(const QString& text) {
    d->field->setPlaceholderText(text);
}

void M3LockSurface::open(Opening opening) {
    d->exiting = false;
    this->relayout();
    if (opening == Opening::FromBoot) {
        // Hold everything back: the choreography draws the hand-over from
        // Plymouth, then the real widgets take its place
        for (QWidget* w : {static_cast<QWidget*>(d->clock), static_cast<QWidget*>(d->date), static_cast<QWidget*>(d->card), static_cast<QWidget*>(d->field), static_cast<QWidget*>(d->actions)}) w->hide();
        d->choreography->avatar = d->card->pictureRect().translated(d->card->pos());
        d->choreography->markMorph = std::make_unique<m3shapes::Morph>(MaterialShapes::sunny(), d->card->pictureShape());
        d->choreography->field = QRectF(d->field->geometry());
        d->choreography->picture = d->picturePath.isEmpty() ? QImage() : QImage(d->picturePath);
        d->choreography->initial = d->displayName.left(1).toUpper();
        d->choreography->setGeometry(this->rect());
        d->horizon->playIntro();
        d->choreography->start(Choreography::Boot);
    } else {
        // Locking: the desktop's wallpaper is already there (the lock wears
        // it as it is), so no rising horizon
        d->card->playEntrance();
        this->focusPassword();
    }
}

QString M3LockSurface::password() const {
    return d->field->text();
}

void M3LockSurface::clearPassword() {
    d->field->clear();
}

void M3LockSurface::setBusy(bool busy) {
    d->field->setEnabled(!busy);
    d->go->setEnabled(!busy);
    if (!busy) this->focusPassword();
}

void M3LockSurface::showError(const QString& text) {
    QPalette pal = d->message->palette();
    pal.setColor(QPalette::WindowText, M3::currentScheme().error);
    d->message->setPalette(pal);
    d->message->setText(text);
    d->shake->stop();
    d->shake->start();
    this->relayout();
}

void M3LockSurface::showMessage(const QString& text) {
    QPalette pal = d->message->palette();
    pal.setColor(QPalette::WindowText, M3::currentScheme().onSurfaceVariant);
    d->message->setPalette(pal);
    d->message->setText(text);
    this->relayout();
}

void M3LockSurface::focusPassword() {
    d->field->setFocus(Qt::OtherFocusReason);
}

void M3LockSurface::playExitToSplash() {
    if (d->exiting) return;
    d->exiting = true;
    d->choreography->field = QRectF(d->field->geometry());
    d->field->hide();
    d->message->hide();
    d->choreography->setGeometry(this->rect());
    d->choreography->start(Choreography::Exit);
}

void M3LockSurface::playExitToDesktop() {
    if (d->exiting) return;
    d->exiting = true;
    // The shape the loading screen opens with, growing from the field until
    // even its dips (0.243 of its box) are past the farthest corner. A window
    // mask, so it works without a compositor.
    const QPointF centre = QRectF(d->field->geometry()).center();
    double farthest = 0;
    for (const QPointF& corner : {QPointF(0, 0), QPointF(this->width(), 0), QPointF(0, this->height()), QPointF(this->width(), this->height())}) {
        farthest = qMax(farthest, QLineF(centre, corner).length());
    }
    const double reach = farthest / 0.243 * 1.1;
    static const QEasingCurve accelerate = [] {
        QEasingCurve c(QEasingCurve::BezierSpline); // M3 "emphasized accelerate"
        c.addCubicBezierSegment(QPointF(0.3, 0), QPointF(0.8, 0.15), QPointF(1, 1));
        return c;
    }();
    if (!d->irisTimer) {
        d->irisTimer = new QTimer(this);
        d->irisTimer->setInterval(16);
        d->irisTimer->setTimerType(Qt::PreciseTimer);
    }
    d->irisTimer->disconnect();
    d->irisClock.start();
    connect(d->irisTimer, &QTimer::timeout, this, [this, centre, reach] {
        const double progress = qMin(1.0, d->irisClock.elapsed() / 650.0);
        const double side = 56 + (reach - 56) * accelerate.valueForProgress(progress);
        const QRectF box(centre.x() - side / 2, centre.y() - side / 2, side, side);
        const QPainterPath hole = M3LoadingIndicator::indeterminatePath(box, d->irisClock.elapsed());
        QRegion mask = QRegion(this->rect()).subtracted(QRegion(hole.toFillPolygon().toPolygon()));
        // An empty region means "no mask" to Qt: keep one pixel outside instead
        QWidget* window = this->window();
        if (mask.isEmpty()) window->setMask(QRegion(-2, -2, 1, 1));
        else window->setMask(mask.translated(this->mapTo(window, QPoint(0, 0))));
        if (progress >= 1) {
            d->irisTimer->stop();
            emit exitFinished();
        }
    });
    d->irisTimer->start();
}

qint64 M3LockSurface::indicatorElapsed() const {
    return d->choreography->mode == Choreography::Exit ? qint64(d->choreography->seconds() * 1000) : 0;
}

void M3LockSurface::writeSplashHandoff(QWidget* window, qint64 indicatorMs) {
    Q_UNUSED(window)
#ifdef HAVE_X11
    if (!tX11Info::isPlatformX11()) return;
    Display* display = tX11Info::display();
    const Window root = tX11Info::appRootWindow();
    // [indicator ms, unix time]: startdesk only trusts a recent one
    long data[2] = {long(indicatorMs), long(QDateTime::currentSecsSinceEpoch())};
    XChangeProperty(display, root, XInternAtom(display, "_THEDESK_SPLASH_HANDOFF", False), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char*>(data), 2);
    // The root window keeps the loading screen's colour after the greeter
    // has gone, so nothing flashes before the loading screen maps
    const QColor surface = M3::currentScheme().surface;
    XSetWindowBackground(display, root, (unsigned long) (surface.rgb() & 0xffffff));
    XClearWindow(display, root);
    XFlush(display);
#else
    Q_UNUSED(indicatorMs)
#endif
}

void M3LockSurface::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    this->relayout();
}

bool M3LockSurface::eventFilter(QObject* watched, QEvent* event) {
    if (watched == d->field && event->type() == QEvent::Resize) {
        d->go->move(d->field->width() - d->go->width() - 8, (d->field->height() - d->go->height()) / 2);
    }
    // The note came or went: the clock's quiet area follows
    if (watched == d->note && (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent)) {
        QTimer::singleShot(0, this, &M3LockSurface::relayout);
    }
    return QWidget::eventFilter(watched, event);
}

void M3LockSurface::setClockNote(QWidget* note) {
    d->note = note;
    note->setParent(this);
    note->installEventFilter(this);
    note->raise();
    this->relayout();
}

void M3LockSurface::relayout() {
    const QRect r = this->rect();
    d->horizon->setGeometry(r);
    d->choreography->setGeometry(r);

    // The clock and date, top left (the lock screen's place for them);
    // smaller on small screens
    const int margin = r.width() < 900 ? 32 : 64;
    const int clockPx = qBound(64, r.height() * 120 / 768, 120);
    if (d->clockPx != clockPx) {
        d->clockPx = clockPx;
        d->clock->setPixelSize(clockPx);
    }
    const QSize clockSize = d->clock->sizeHint();
    const int clockTop = r.height() < 600 ? 24 : 48;
    d->clock->setGeometry(margin, clockTop, clockSize.width(), clockSize.height());
    d->date->setGeometry(margin + 2, clockTop + clockSize.height() + 8, qMax(200, r.width() / 2), 30);
    QRect clockArea(margin, clockTop, qMax(clockSize.width(), d->date->sizeHint().width()), clockSize.height() + 38);
    if (d->note) {
        const QSize noteSize = d->note->sizeHint();
        d->note->setGeometry(margin + 2 - 10, clockArea.bottom() + 8, noteSize.width(), noteSize.height());
        if (!d->note->isHidden()) {
            clockArea.setHeight(clockArea.height() + 8 + noteSize.height());
            clockArea.setWidth(qMax(clockArea.width(), noteSize.width() - 8));
        }
    }
    d->horizon->setQuietRect(QRectF(clockArea).adjusted(-24, -16, 24, 16));

    // The card and the field: a column a little above the middle, below
    // the clock when the screen is too narrow for both side by side
    const QSize cardSize = d->card->sizeHint().expandedTo(QSize(320, 0));
    const int columnHeight = cardSize.height() + 20 + 56 + 12 + 48; // card, field, message
    int centreX = r.width() / 2;
    int top = qMax(clockTop + clockSize.height() / 2, int(r.height() * 0.26));
    const QRect cardRect(centreX - cardSize.width() / 2, top, cardSize.width(), cardSize.height());
    if (cardRect.intersects(clockArea.adjusted(0, 0, 16, 0))) {
        if (clockArea.bottom() + 16 + columnHeight <= r.height() - 16) {
            top = clockArea.bottom() + 16; // below the clock
        } else {
            // Short too: beside the clock, centred in what's left
            centreX = (clockArea.right() + 24 + r.width()) / 2;
            top = qMax(16, (r.height() - columnHeight) / 2);
        }
    }
    d->card->setGeometry(centreX - cardSize.width() / 2, top, cardSize.width(), cardSize.height());
    d->field->setGeometry(centreX - 160, d->card->geometry().bottom() + 20, 320, 56);
    d->message->setGeometry(centreX - 200, d->field->geometry().bottom() + 12, 400, 48);

    const QSize actionsSize = d->actions->sizeHint();
    d->actions->setGeometry(r.width() - 48 - actionsSize.width(), r.height() - 40 - actionsSize.height(), actionsSize.width(), actionsSize.height());
    d->choreography->raise();
}
