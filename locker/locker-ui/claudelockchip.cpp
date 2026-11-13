/****************************************
 *
 *   theDesk - Claude on the lock screen
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "claudelockchip.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <claude/claudeusage.h>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    constexpr int Height = 32;
    constexpr int BeatMs = 1600;

    enum State {
        Nothing,
        Working,
        Waiting,
        Done
    };

    QFont label() {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(14);
        font.setVariableAxis(QFont::Tag("wght"), 560);
        return font;
    }
} // namespace

struct ClaudeLockChip::Private {
        QDateTime lockedAt;
        State state = Nothing;
        QString text;
        // It fades in and out, and fills while Claude waits: effects both
        M3SpringAnimation* shown;
        M3SpringAnimation* fill;
        // Working: the logo's beat
        QTimer beat;
        QElapsedTimer clock;
        double phase = -1;
};

ClaudeLockChip::ClaudeLockChip(QWidget* parent) :
    QWidget(parent) {
    d = new Private();
    d->lockedAt = QDateTime::currentDateTimeUtc();
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->hide();

    d->shown = new M3SpringAnimation(this);
    d->shown->setToken(M3Motion::EffectsDefault);
    d->shown->snapTo(0.0);
    connect(d->shown, &M3SpringAnimation::valueChanged, this, [this](const QVariant& value) {
        if (d->state == Nothing && value.toDouble() < 0.01) this->hide();
        this->update();
    });
    d->fill = new M3SpringAnimation(this);
    d->fill->setToken(M3Motion::EffectsDefault);
    d->fill->snapTo(0.0);
    connect(d->fill, &M3SpringAnimation::valueChanged, this, [this] {
        this->update();
    });

    // The beat ends where the logo is whole, so work that stops mid-beat
    // lets it finish first
    d->beat.setInterval(33);
    connect(&d->beat, &QTimer::timeout, this, [this] {
        const double phase = (d->clock.elapsed() % BeatMs) / double(BeatMs);
        if (phase < d->phase && d->state != Working) {
            d->beat.stop();
            d->phase = -1;
        } else {
            d->phase = phase;
        }
        this->update();
    });

    connect(ClaudeUsage::instance(), &ClaudeUsage::changed, this, &ClaudeLockChip::follow);
    this->follow();
}

ClaudeLockChip::~ClaudeLockChip() {
    delete d;
}

QSize ClaudeLockChip::sizeHint() const {
    return QSize(10 + 18 + 8 + QFontMetrics(label()).horizontalAdvance(d->text) + 14, Height);
}

void ClaudeLockChip::follow() {
    ClaudeUsage* usage = ClaudeUsage::instance();
    State state = Nothing;
    if (usage->activity() == ClaudeUsage::Waiting) {
        state = Waiting;
    } else if (usage->activity() == ClaudeUsage::Working) {
        state = Working;
    } else if (usage->finished().isValid() && usage->finished() >= d->lockedAt) {
        // Only a turn that ended while the screen was locked is news here
        state = Done;
    }
    if (state == d->state) return;
    d->state = state;

    if (state != Nothing) {
        d->text = state == Waiting ? tr("Claude needs you") : state == Working ? tr("Claude is working") : tr("Claude is done");
        this->resize(this->sizeHint());
        this->show();
    }
    d->shown->animateTo(state == Nothing ? 0.0 : 1.0);
    d->fill->animateTo(state == Waiting ? 1.0 : 0.0);
    // The rays move while it works; calmer motion leaves the logo still
    if (state == Working && M3Motion::expressive() && !d->beat.isActive()) {
        d->clock.start();
        d->phase = 0;
        d->beat.start();
    }
    this->update();
}

void ClaudeLockChip::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(qBound(0.0, d->shown->currentValue().toDouble(), 1.0));
    const double fill = qBound(0.0, d->fill->currentValue().toDouble(), 1.0);
    const double cy = this->height() / 2.0;

    if (fill > 0) {
        QColor container = s.tertiaryContainer;
        container.setAlphaF(fill);
        p.setPen(Qt::NoPen);
        p.setBrush(container);
        p.drawRoundedRect(QRectF(0, cy - Height / 2.0, this->width(), Height), Height / 2.0, Height / 2.0);
    }

    ClaudeUsage::paintLogo(&p, QRectF(10, cy - 9, 18, 18), d->phase);

    p.setFont(label());
    p.setPen(M3::layer(s.onSurface, s.onTertiaryContainer, fill));
    p.drawText(QRectF(10 + 18 + 8, 0, this->width(), this->height()), Qt::AlignVCenter | Qt::AlignLeft, d->text);
}
