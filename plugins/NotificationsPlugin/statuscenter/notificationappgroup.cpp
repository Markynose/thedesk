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
#include "notificationappgroup.h"

#include "jobwidget.h"
#include "notificationwidget.h"
#include "pebble.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <shapes/morph.h>

namespace {
    constexpr double Header = 18;    // the app's name, over the cards
    constexpr double CardsLeft = 84; // the shape's column
    constexpr double ShapeSize = 56;
    constexpr double Gap = 6;        // between dealt cards

    double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }
    QRectF lerp(const QRectF& a, const QRectF& b, double t) {
        return QRectF(QPointF(lerp(a.left(), b.left(), t), lerp(a.top(), b.top(), t)), QPointF(lerp(a.right(), b.right(), t), lerp(a.bottom(), b.bottom(), t)));
    }
} // namespace

struct NotificationAppGroupPrivate {
        ApplicationPointer application;
        QString name;
        QPainterPath shape;
        std::unique_ptr<m3shapes::Morph> opened; // its shape, into a 12-sided cookie as the deck deals
        QImage glyph;
        QColor bg, fg;

        // jobs first, then notifications, newest first
        struct Card {
                QPointer<QWidget> widget;
                NotificationPtr notification;
                bool job = false, leaving = false;
                Pebble::Spring dealt{0}, suck{0};
        };
        QList<Card> cards;
        QToolButton* deal;
        bool open = false, emptied = false;
        Pebble::Spring openK{0}, rot{-120, M3Motion::SpatialSlow}, scale{0}, height{0}, kept{1};

        QTimer frames;
        QElapsedTimer clock;
        qint64 lastFrame = 0;
};

NotificationAppGroup::NotificationAppGroup(ApplicationPointer application, QWidget* parent) :
    QWidget(parent) {
    d = new NotificationAppGroupPrivate();
    d->application = application;
    d->name = application ? application->getProperty("Name").toString() : tr("Uncategorised");
    const QString key = Pebble::key(application);
    const M3::Scheme s = M3::currentScheme();
    d->shape = m3shapes::toPath(Pebble::shape(key));
    d->opened = std::make_unique<m3shapes::Morph>(Pebble::shape(key), MaterialShapes::cookie12Sided());
    Pebble::colours(key, s, &d->bg, &d->fg);
    d->glyph = Pebble::glyph(application, 26, this->devicePixelRatioF(), d->bg, d->fg);

    // The shape is a button: it deals the deck out (and back)
    d->deal = new QToolButton(this);
    d->deal->setAutoRaise(true);
    d->deal->setFixedSize(int(ShapeSize), int(ShapeSize));
    d->deal->setProperty("m3HoverMerge", false);
    d->deal->setAccessibleName(d->name);
    d->deal->setToolTip(tr("Show all"));
    connect(d->deal, &QToolButton::clicked, this, [this] {
        if (d->cards.count() < 2) return;
        d->open = !d->open;
        d->deal->setToolTip(d->open ? tr("Stack") : tr("Show all"));
        animate();
    });

    // it pops in as its shape
    d->scale.target = 1;
    d->rot.target = 0;
    d->frames.setInterval(16);
    d->clock.start();
    connect(&d->frames, &QTimer::timeout, this, &NotificationAppGroup::frame);
    this->setFixedHeight(int(Header + ShapeSize + 24));
    animate();
}

NotificationAppGroup::~NotificationAppGroup() {
    // its cards go after this (in ~QWidget): their signals mustn't reach d
    for (const auto& card : std::as_const(d->cards)) {
        if (card.widget) card.widget->disconnect(this);
        if (card.notification) card.notification->disconnect(this);
    }
    delete d;
}

void NotificationAppGroup::pushNotification(NotificationPtr notification) {
    NotificationWidget* w = new NotificationWidget(notification, this);
    connect(w, &NotificationWidget::closeRequested, this, [this, notification] {
        // A stacked deck goes as one; a dealt one card by card
        if (!d->open && d->cards.count() > 1) dismissAll();
        else notification->dismiss(Notification::NotificationUserDismissed);
    });
    connect(notification, &Notification::dismissed, this, [this, w] {
        // pulled back into the shape
        for (auto& card : d->cards) {
            if (card.widget == w) card.leaving = true, card.suck.target = 1;
        }
        animate();
    });
    int jobs = 0;
    while (jobs < d->cards.count() && d->cards.at(jobs).job) jobs++;
    NotificationAppGroupPrivate::Card card;
    card.widget = w;
    card.notification = notification;
    card.dealt.snap(d->open ? 1 : 0);
    d->cards.insert(jobs, card);
    w->show();
    restack();
    animate();
}

void NotificationAppGroup::pushJob(SystemJobPtr job) {
    auto w = new JobWidget(job, this);
    connect(w, &JobWidget::destroyed, this, &NotificationAppGroup::animate);
    NotificationAppGroupPrivate::Card card;
    card.widget = w;
    card.job = true;
    card.dealt.snap(d->open ? 1 : 0);
    d->cards.prepend(card);
    w->show();
    restack();
    animate();
}

void NotificationAppGroup::dismissAll() {
    for (const auto& card : std::as_const(d->cards)) {
        if (card.notification) card.notification->dismiss(Notification::NotificationUserDismissed);
    }
}

void NotificationAppGroup::restack() {
    // the top card sits over the ones tucked under it
    for (int i = d->cards.count() - 1; i >= 0; i--) {
        if (d->cards.at(i).widget) d->cards.at(i).widget->raise();
    }
    d->deal->raise();
}

QPointF NotificationAppGroup::shapeCentre() const {
    return QPointF(CardsLeft / 2, Header + 34);
}

void NotificationAppGroup::animate() {
    if (d->frames.isActive()) return;
    d->lastFrame = d->clock.elapsed() - 16;
    d->frames.start();
}

void NotificationAppGroup::showEvent(QShowEvent* event) {
    Q_UNUSED(event)
    animate();
}

void NotificationAppGroup::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event)
    animate();
}

void NotificationAppGroup::frame() {
    const qint64 now = d->clock.elapsed();
    const double dt = qBound(0.001, (now - d->lastFrame) / 1000.0, 0.05);
    d->lastFrame = now;

    // cards that went: a finished job, or one pulled all the way in
    for (int i = d->cards.count() - 1; i >= 0; i--) {
        auto& card = d->cards[i];
        if (!card.widget || (card.leaving && card.suck.value > 0.97)) {
            if (card.widget) card.widget->deleteLater();
            d->cards.removeAt(i);
        }
    }
    int count = 0;
    for (const auto& card : std::as_const(d->cards)) count += card.leaving ? 0 : 1;
    if (count < 2) d->open = false;
    if (d->cards.isEmpty() && !d->emptied) {
        // nothing left: the shape spins away, then the gap closes
        d->emptied = true;
        d->scale.target = 0;
        d->rot.target += 140;
        emit emptied();
        QTimer::singleShot(120, this, [this] {
            d->kept.target = 0;
            animate();
        });
    }
    d->openK.target = d->open ? 1 : 0;
    if (!d->emptied) d->rot.target = d->open ? 40 : 0;

    bool moving = false;
    for (Pebble::Spring* spring : {&d->openK, &d->rot, &d->scale, &d->kept}) moving |= spring->step(dt);

    // The deck: stacked, the cards after the top one peek out under it
    // (two at most); dealt, they sit one under another
    const double width = this->width() - CardsLeft;
    const QPointF c = shapeCentre();
    double dealtY = Header, bottom = Header + ShapeSize + 24, topHeight = 0;
    for (int k = 0; k < d->cards.count(); k++) {
        auto& card = d->cards[k];
        card.dealt.target = k == 0 || d->open ? 1 : 0;
        moving |= card.dealt.step(dt);
        moving |= card.suck.step(dt);
        const double cardHeight = card.widget->heightForWidth(int(width));
        if (k == 0) topHeight = cardHeight;
        const double e = k == 0 ? 1 : qBound(0.0, card.dealt.value, 1.2);
        const int peek = qMin(k, 2);
        const double inset = (1 - qMin(1.0, e)) * 14 * peek;
        QRectF r(CardsLeft + inset, lerp(Header + 8 * peek, dealtY, e), width - 2 * inset, lerp(topHeight, cardHeight, qMin(1.0, e)));
        r = lerp(r, QRectF(c.x() - 20, c.y() - 20, 40, 40), qBound(0.0, card.suck.value, 1.0));
        card.widget->setGeometry(r.toRect());
        // tucked under, it's only a sliver: its words show once it's out
        card.widget->setVisible(card.suck.value < 0.6 && (k == 0 || e > 0.3));
        if (auto* notification = qobject_cast<NotificationWidget*>(card.widget)) notification->setDealt(k == 0 ? 1 : e);
        if (!card.leaving) {
            dealtY += cardHeight + Gap;
            bottom = qMax(bottom, r.bottom() + 2);
        }
    }
    if (d->height.value == 0) d->height.snap(bottom);
    d->height.target = bottom;
    moving |= d->height.step(dt);
    this->setFixedHeight(qMax(0, qCeil(d->height.value * qBound(0.0, d->kept.value, 1.0))));
    d->deal->move(qRound(c.x() - ShapeSize / 2), qRound(c.y() - ShapeSize / 2));
    d->deal->setVisible(!d->emptied);
    this->update();

    if (d->emptied && d->kept.value < 0.01 && d->kept.target == 0) {
        d->frames.stop();
        this->deleteLater();
        return;
    }
    // a job's ring waves while it runs
    bool job = false;
    for (const auto& card : std::as_const(d->cards)) job |= card.job;
    if (!moving && !(job && this->isVisible())) d->frames.stop();
}

void NotificationAppGroup::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QPointF c = shapeCentre();
    const double scale = qMax(0.0, d->scale.value), k = qBound(0.0, d->openK.value, 1.0);

    p.setOpacity(qBound(0.0, scale * 2, 1.0));
    p.setFont(Pebble::flex(12, 600));
    p.setPen(s.onSurfaceVariant);
    p.drawText(QRectF(CardsLeft + 4, 0, this->width() - CardsLeft - 4, Header - 2), Qt::AlignLeft | Qt::AlignVCenter, d->name);
    p.setOpacity(1);

    // the cards still tucked under the top one
    for (int i = 1; i < d->cards.count(); i++) {
        QWidget* w = d->cards.at(i).widget;
        if (!w || w->isVisible() || d->cards.at(i).leaving) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(s.surfaceContainerHighest);
        p.drawRoundedRect(QRectF(w->geometry()), 20, 20);
    }

    // a running job: a wavy ring round the shape
    JobWidget* job = d->cards.isEmpty() ? nullptr : qobject_cast<JobWidget*>(d->cards.constFirst().widget);
    if (job && scale > 0.5) {
        const double frac = job->progress(), phase = d->clock.elapsed() / 1000.0 * 5, radius = 37;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(s.secondaryContainer, 4, Qt::SolidLine, Qt::RoundCap));
        const double gap = 14, a0 = -90 + 360 * frac + gap, a1 = 270 - gap;
        if (a1 > a0) p.drawArc(QRectF(c.x() - radius, c.y() - radius, 2 * radius, 2 * radius), int(-a0 * 16), int(-(a1 - a0) * 16));
        QPainterPath wave;
        const int n = qMax(2, int(360 * frac));
        for (int i = 0; i <= n; i++) {
            const double th = qDegreesToRadians(-90 + 360 * frac * i / n);
            const double r = radius + 2 * std::sin(th * 10 + phase);
            const QPointF pt(c.x() + r * std::cos(th), c.y() + r * std::sin(th));
            if (i == 0) wave.moveTo(pt);
            else wave.lineTo(pt);
        }
        p.setPen(QPen(s.primary, 4, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(wave);
    }

    // the shape (a 12-sided cookie while the deck is dealt)
    const double size = ShapeSize * scale * (1 + 0.08 * k);
    p.setPen(Qt::NoPen);
    p.fillPath(Pebble::shapePath(m3shapes::toPath(*d->opened, float(k)), c, size, d->rot.value), d->bg);
    if (scale > 0.2) Pebble::drawCentred(p, c, d->glyph, 26 * scale);

    // how many; a chevron once they're dealt
    int count = 0;
    for (const auto& card : std::as_const(d->cards)) count += card.leaving ? 0 : 1;
    if (count > 1 && scale > 0.5) {
        const QPointF badge(c.x() + 20, c.y() + 20);
        p.setBrush(s.primary);
        p.drawEllipse(badge, 11, 11);
        const QRectF box(badge.x() - 11, badge.y() - 11, 22, 22);
        p.setPen(s.onPrimary);
        p.setOpacity(1 - k);
        p.setFont(Pebble::flex(12, 700));
        p.drawText(box, Qt::AlignCenter, QString::number(count));
        p.setOpacity(k);
        p.setFont(Pebble::symbols(16));
        p.drawText(box, Qt::AlignCenter, QStringLiteral("expand_less"));
    }
}
