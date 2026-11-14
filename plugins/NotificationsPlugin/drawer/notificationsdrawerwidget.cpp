/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "notificationsdrawerwidget.h"

#include "notification.h"
#include "notificationtracker.h"
#include "pebble.h"
#include <QCursor>
#include <QElapsedTimer>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <shapes/morph.h>
#include <tvariantanimation.h>

namespace {
    constexpr double Folded = 72;   // a pebble in the rail
    constexpr double ShapeSize = 56;
    constexpr double TextLeft = 86; // the card's words, right of the shape
    constexpr double Gap = 10;      // between pebbles

    double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }
    QColor mix(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(float(lerp(a.redF(), b.redF(), t)), float(lerp(a.greenF(), b.greenF(), t)), float(lerp(a.blueF(), b.blueF(), t)));
    }
    QPainterPath rounded(const QRectF& r, double radius) {
        QPainterPath path;
        path.addRoundedRect(r, radius, radius);
        return path;
    }
} // namespace

struct NotificationsDrawerWidgetPrivate {
        NotificationTracker* tracker;
        tVariantAnimation* timeout;
        NotificationPtr n;
        bool shouldTimeoutRun = false;

        QWidget* content;
        QLabel *app, *summary, *body;
        QWidget* actions;
        QList<QPushButton*> buttons;
        QGraphicsOpacityEffect* contentOpacity;
        QToolButton* close;
        double cardHeight = 88;

        QString key;
        QPainterPath shape; // the app's, normalised
        QImage glyph;
        QColor bg, fg;

        Pebble::Spring hk{0}, u{0}, enter{0}, x{0}, morph{0};
        Pebble::Spring rot{-120, M3Motion::SpatialSlow}, alpha{0, M3Motion::EffectsDefault};
        QElapsedTimer born;
        bool front = false, hovered = false;
        bool gone = false, answered = false, folded = false;
        QTimer turn; // a critical one keeps turning while it waits

        // swipe sideways to dismiss
        bool pressed = false, swiping = false, flung = false;
        double pressX = 0, lastX = 0, velocity = 0;
        QElapsedTimer moveClock;
};

NotificationsDrawerWidget::NotificationsDrawerWidget(NotificationPtr notification, NotificationTracker* tracker, QWidget* parent) :
    QWidget(parent) {
    d = new NotificationsDrawerWidgetPrivate();
    d->n = notification;
    d->tracker = tracker;
    const M3::Scheme s = M3::currentScheme();

    // The card's words and actions are real widgets; the rest is painted
    d->content = new QWidget(this);
    auto* layout = new QVBoxLayout(d->content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto label = [&](const QFont& font, const QColor& colour) {
        auto* l = new QLabel(d->content);
        l->setFont(font);
        QPalette pal = l->palette();
        for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, colour);
        l->setPalette(pal);
        l->setWordWrap(true);
        layout->addWidget(l);
        return l;
    };
    d->app = label(Pebble::flex(12, 500), s.onSurfaceVariant);
    d->summary = label(Pebble::flex(16, 600), s.onSurface);
    d->body = label(Pebble::flex(14, 400), s.onSurfaceVariant);
    d->app->setContentsMargins(0, 0, 36, 0); // the close button's corner
    d->summary->setContentsMargins(0, 0, 36, 0);
    d->actions = new QWidget(d->content);
    d->actions->setProperty("m3ButtonGroup", QStringLiteral("connected"));
    auto* actionsLayout = new QHBoxLayout(d->actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(2);
    layout->addSpacing(8);
    layout->addWidget(d->actions, 0, Qt::AlignLeft);
    d->contentOpacity = new QGraphicsOpacityEffect(d->content);
    d->content->setGraphicsEffect(d->contentOpacity);
    d->content->hide();

    d->close = new QToolButton(this);
    d->close->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    d->close->setAutoRaise(true);
    d->close->setProperty("m3ButtonSize", QStringLiteral("xs"));
    d->close->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
    d->close->setToolTip(tr("Dismiss"));
    d->close->hide();
    connect(d->close, &QToolButton::clicked, this, [this] {
        d->n->dismiss(Notification::NotificationUserDismissed);
    });

    d->summary->setText(notification->summary());
    d->body->setText(notification->body());
    d->body->setVisible(!notification->body().isEmpty());
    connect(notification, &Notification::summaryChanged, this, [this](QString summary) {
        d->summary->setText(summary);
        measure();
    });
    connect(notification, &Notification::bodyChanged, this, [this](QString body) {
        d->body->setText(body);
        d->body->setVisible(!body.isEmpty());
        measure();
    });
    connect(notification, &Notification::dismissed, this, [this] {
        if (!d->answered) animateDismiss(); // answered: it leaves once the check has shown
    });

    d->timeout = new tVariantAnimation(this);
    d->timeout->setStartValue(1.0);
    d->timeout->setEndValue(0.0);
    d->timeout->setEasingCurve(QEasingCurve::Linear);
    d->timeout->setForceAnimation(true);
    d->timeout->setDuration(notification->timeout() == 0 ? -1 : notification->timeout());
    connect(d->timeout, &tVariantAnimation::finished, this, &NotificationsDrawerWidget::animateDismiss);
    connect(d->timeout, &tVariantAnimation::valueChanged, this, [this] {
        // only the countdown ring moves
        const QPointF c = coin();
        this->update(QRectF(c.x() - 36, c.y() - 36, 72, 72).toAlignedRect());
    });
    connect(d->tracker, &NotificationTracker::pauseTimeouts, this, [this] {
        d->timeout->pause();
    });
    connect(d->tracker, &NotificationTracker::resumeTimeouts, this, [this] {
        if (d->shouldTimeoutRun && d->n->timeout() != 0 && !needsAnswer()) d->timeout->start();
    });
    connect(notification, &Notification::timeoutChanged, this, [this](qint32 timeout) {
        d->timeout->setDuration(timeout == 0 || needsAnswer() ? -1 : timeout);
    });

    d->turn.setInterval(700);
    connect(&d->turn, &QTimer::timeout, this, [this] {
        if (d->answered || d->gone || !needsAnswer()) return;
        d->rot.target += 45;
        emit animate();
    });
    connect(notification, &Notification::applicationChanged, this, &NotificationsDrawerWidget::updateApplication);
    connect(notification, &Notification::actionsChanged, this, &NotificationsDrawerWidget::setupActions);
    setupActions();

    // It pops in as its shape (a scale and a spin), then unfurls
    d->hk.target = 1;
    d->enter.target = 1;
    d->alpha.target = 1;
    d->rot.target = 0;
    d->born.start();
    QTimer::singleShot(220, this, [this] {
        emit animate();
    });
    QTimer::singleShot(500, this, [this] {
        d->shouldTimeoutRun = true;
        if (d->n->timeout() != 0 && !needsAnswer() && !d->swiping && !d->flung && !d->gone && !d->hovered) d->timeout->start();
    });
}

NotificationsDrawerWidget::~NotificationsDrawerWidget() {
    // ~QWidget can still send enter events (and with them the tracker's
    // pause) after this: nothing may reach d once it's gone
    d->tracker->disconnect(this);
    if (d->n) d->n->disconnect(this);
    delete d;
}

void NotificationsDrawerWidget::updateApplication() {
    const M3::Scheme s = M3::currentScheme();
    const bool critical = d->n->urgency() == Notification::Critical;
    d->key = Pebble::key(d->n->application());
    d->shape = m3shapes::toPath(Pebble::shape(d->key));
    if (critical) {
        d->bg = s.errorContainer;
        d->fg = s.onErrorContainer;
    } else {
        Pebble::colours(d->key, s, &d->bg, &d->fg);
    }
    d->glyph = Pebble::glyph(d->n->application(), 26, this->devicePixelRatioF(), d->bg, d->fg);

    const QString name = d->n->application() ? d->n->application()->getProperty(QStringLiteral("Name")).toString() : QString();
    d->app->setText(needsAnswer() ? tr("%1 · needs an answer").arg(name) : tr("%1 · now").arg(name));
    QPalette pal = d->app->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, needsAnswer() ? s.error : s.onSurfaceVariant);
    d->app->setPalette(pal);
    this->update();
}

void NotificationsDrawerWidget::setupActions() {
    qDeleteAll(d->buttons);
    d->buttons.clear();
    const bool critical = d->n->urgency() == Notification::Critical;
    for (const Notification::Action& action : d->n->actions()) {
        auto* button = new QPushButton(d->actions);
        button->setText(action.text);
        button->setIcon(action.icon);
        button->setProperty("m3ButtonSize", QStringLiteral("xs"));
        // a critical one's answer is the filled one
        button->setProperty("m3ButtonStyle", critical && d->buttons.isEmpty() ? QStringLiteral("filled") : QStringLiteral("tonal"));
        connect(button, &QPushButton::clicked, this, [this, action] {
            if (d->gone || d->answered) return;
            emit d->n->actionInvoked(action);
            if (d->n->urgency() != Notification::Critical) {
                animateDismiss();
                return;
            }
            // Answered: its shape morphs into a circle with a check, then it
            // folds and goes
            d->answered = true;
            d->timeout->stop();
            d->morph.target = 1;
            d->actions->setEnabled(false);
            emit animate();
            QTimer::singleShot(100, this, [this] {
                d->folded = true;
                emit animate();
            });
            QTimer::singleShot(450, this, &NotificationsDrawerWidget::animateDismiss);
        });
        d->actions->layout()->addWidget(button);
        d->buttons.append(button);
    }
    d->actions->setVisible(!d->buttons.isEmpty());
    // A critical notification with actions (a pairing code, a call) waits
    // for its answer: it stays unfurled, its buttons showing
    if (needsAnswer()) {
        d->timeout->setDuration(-1);
        d->turn.start();
    }
    updateApplication();
    measure();
}

void NotificationsDrawerWidget::measure() {
    const int width = NotificationsDrawerWidget::CardWidth - TextLeft - 16;
    d->content->setFixedWidth(width);
    d->content->layout()->activate();
    const int height = d->content->layout()->totalHeightForWidth(width);
    d->content->setFixedHeight(height); // the window's mask cuts it to the plate as it unfurls
    d->cardHeight = qMax(88, 12 + height + 14);
    emit animate();
}

bool NotificationsDrawerWidget::needsAnswer() const {
    return d->n->urgency() == Notification::Critical && !d->n->actions().isEmpty();
}

void NotificationsDrawerWidget::setFront(bool front) {
    d->front = front;
}

bool NotificationsDrawerWidget::leaving() const {
    return d->gone;
}

bool NotificationsDrawerWidget::done() const {
    return d->gone && d->hk.target == 0 && d->hk.value < 0.01;
}

void NotificationsDrawerWidget::animateDismiss() {
    if (d->gone) return;
    d->gone = true;
    d->shouldTimeoutRun = false;
    d->timeout->stop();
    d->close->hide();
    if (!d->flung) {
        // it folds and spins away to nothing
        d->alpha.target = 0;
        d->enter.target = 0;
        d->rot.target += 120;
    }
    QTimer::singleShot(200, this, [this] {
        d->hk.target = 0;
        emit animate();
    });
    emit animate();
}

QRectF NotificationsDrawerWidget::plate() const {
    const double u = d->u.value;
    const double w = lerp(Folded, CardWidth, u), h = lerp(Folded, d->cardHeight, u);
    return QRectF(this->width() - Edge - w + d->x.value, 0, w, h);
}

QPointF NotificationsDrawerWidget::coin() const {
    const QRectF p = plate();
    const double u = d->u.value;
    return QPointF(p.left() + 36 + 8 * u, p.top() + 36 + 8 * u);
}

int NotificationsDrawerWidget::plateHeight() const {
    return qCeil(lerp(Folded, d->cardHeight, qMax(0.0, d->u.value))) + 1;
}

double NotificationsDrawerWidget::slot() const {
    return (lerp(Folded, d->cardHeight, d->u.value) + Gap) * d->hk.value;
}

QPainterPath NotificationsDrawerWidget::outline() const {
    const double sc = qMax(0.0, d->enter.value);
    if (sc < 0.01 || (d->gone && d->alpha.value < 0.01 && !d->flung)) return {};
    const QPointF c = coin();
    QTransform t;
    t.translate(c.x(), c.y());
    t.scale(sc, sc);
    t.translate(-c.x(), -c.y());
    return t.map(rounded(plate(), lerp(36, 28, d->u.value)));
}

bool NotificationsDrawerWidget::step(double dt) {
    const bool open = d->hovered || needsAnswer() || (d->front && d->born.elapsed() >= 220);
    d->u.target = d->gone || d->folded ? (d->flung ? d->u.target : 0) : (open ? 1 : 0);
    bool moving = false;
    for (Pebble::Spring* spring : {&d->hk, &d->u, &d->enter, &d->x, &d->morph, &d->rot, &d->alpha}) moving |= spring->step(dt);
    if (d->front && d->born.elapsed() < 220) moving = true; // about to unfurl

    // The words show once it's mostly a card
    const QRectF p = plate();
    const double away = d->flung || d->swiping ? qBound(0.0, 1 - qAbs(d->x.value) / CardWidth * 1.2, 1.0) : d->alpha.value;
    const double words = qBound(0.0, (d->u.value - 0.55) / 0.45, 1.0) * away;
    d->content->setVisible(words > 0.01);
    d->contentOpacity->setOpacity(words);
    d->contentOpacity->setEnabled(words < 0.999);
    d->content->move(qRound(p.left() + TextLeft), qRound(p.top() + 12));
    d->close->setVisible(d->hovered && d->u.value > 0.9 && !d->gone);
    d->close->move(qRound(p.right() - 12 - d->close->width()), qRound(p.top() + 12));
    this->update();
    return moving;
}

void NotificationsDrawerWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const double sc = qMax(0.0, d->enter.value), u = d->u.value;
    const double alpha = d->flung || d->swiping ? qBound(0.0, 1 - qAbs(d->x.value) / CardWidth * 1.2, 1.0) : d->alpha.value;
    if (sc < 0.01 || alpha < 0.005) return;
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setOpacity(qBound(0.0, alpha, 1.0));
    const QPointF c = coin();

    // the plate grows out of the pebble
    p.save();
    p.translate(c);
    p.scale(sc, sc);
    p.translate(-c);
    p.fillPath(rounded(plate(), lerp(36, 28, u)), mix(s.surfaceContainer, s.surfaceContainerHigh, u));
    p.restore();

    // its countdown, a ring round the pebble
    if (d->n->timeout() != 0 && d->timeout->state() != QAbstractAnimation::Stopped && !d->gone) {
        const double left = qBound(0.0, d->timeout->currentValue().toDouble(), 1.0);
        p.setPen(QPen(s.primary, 3, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(c.x() - 33, c.y() - 33, 66, 66), 90 * 16, int(left * 360 * 16));
        p.setPen(Qt::NoPen);
    }

    // its shape: a critical one is a soft burst that turns into a check
    if (d->n->urgency() == Notification::Critical) {
        static const m3shapes::Morph answered(MaterialShapes::softBurst(), MaterialShapes::circle());
        const double k = qBound(0.0, d->morph.value, 1.0);
        p.fillPath(Pebble::shapePath(m3shapes::toPath(answered, float(k)), c, ShapeSize * sc, d->rot.value), mix(s.errorContainer, s.primaryContainer, k));
        if (k > 0.5) {
            p.setFont(Pebble::symbols(26 * sc));
            p.setPen(s.onPrimaryContainer);
            p.drawText(QRectF(c.x() - 26, c.y() - 26, 52, 52), Qt::AlignCenter, QStringLiteral("check"));
            return;
        }
    } else {
        p.fillPath(Pebble::shapePath(d->shape, c, ShapeSize * sc, d->rot.value), d->bg);
    }
    if (sc > 0.2) Pebble::drawCentred(p, c, d->glyph, 26 * sc);
}

void NotificationsDrawerWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    d->hovered = true;
    d->tracker->pauseTimeouts();
    emit animate();
}

void NotificationsDrawerWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = false;
    if (!d->swiping) d->tracker->resumeTimeouts();
    emit animate();
}

// Swipe sideways to dismiss: the card follows the pointer and fades; let go
// past a third of its width (or flick it) and it flies off, otherwise it
// springs back. Hovering a pebble in the rail unfurls it
void NotificationsDrawerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || d->flung || d->gone) return;
    d->pressed = true;
    d->swiping = false;
    d->pressX = d->lastX = event->globalPosition().x();
    d->velocity = 0;
    d->moveClock.start();
}

void NotificationsDrawerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!d->pressed) return;
    const double x = event->globalPosition().x();
    const double dx = x - d->pressX;
    if (!d->swiping && qAbs(dx) > 8) {
        d->swiping = true;
        d->tracker->pauseTimeouts();
    }
    if (!d->swiping) return;
    const qint64 ms = qMax<qint64>(1, d->moveClock.restart());
    d->velocity = 0.6 * d->velocity + 0.4 * (x - d->lastX) * 1000.0 / ms;
    d->lastX = x;
    d->x.snap(dx);
    emit animate();
}

void NotificationsDrawerWidget::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event)
    if (!d->pressed) return;
    d->pressed = false;
    if (!d->swiping) return;
    const double dx = d->lastX - d->pressX;
    const double width = plate().width();
    if (qAbs(dx) > width / 3 || qAbs(d->velocity) > 900) {
        // Off it goes, the way it was going, then the gap closes
        d->flung = true;
        const double direction = (qAbs(d->velocity) > 900 ? d->velocity : dx) > 0 ? 1 : -1;
        d->x.target = direction * (width + 40);
        d->tracker->resumeTimeouts();
        QTimer::singleShot(260, this, [this] {
            d->n->dismiss(Notification::NotificationUserDismissed);
            animateDismiss(); // (already dismissed elsewhere: it still goes)
        });
    } else {
        d->x.target = 0;
        // Still hovered: the hover keeps them paused (Leave resumes them)
        if (!this->rect().contains(this->mapFromGlobal(QCursor::pos()))) d->tracker->resumeTimeouts();
    }
    d->swiping = false;
    emit animate();
}
