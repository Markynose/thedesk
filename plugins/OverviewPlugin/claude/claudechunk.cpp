/****************************************
 *
 *   theDesk - Claude's usage limits in the bar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "claudechunk.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <barmanager.h>
#include <claude/claudeusage.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <statemanager.h>
#include <statuscentermanager.h>

namespace {
    constexpr int Height = 26;

    QFont label(const QFont& base) {
        QFont font = base;
        font.setFamily(QStringLiteral("Roboto Flex"));
        font.setPixelSize(14);
        font.setVariableAxis(QFont::Tag("wght"), 560);
        return font;
    }

    QString figure(const ClaudeUsage::Limit& limit) {
        return QLocale().toString(qRound(limit.percent)) + QStringLiteral("%");
    }

    // Past 90% a limit turns to the error colour
    QColor colourFor(const M3::Scheme& s, const ClaudeUsage::Limit& limit, QColor normal) {
        return limit.percent >= 90 ? s.error : normal;
    }
} // namespace

struct ClaudeChunk::Private {
        // Waiting on the person: the fill and the words come in (an effect),
        // the chunk grows to hold them (a move)
        M3SpringAnimation* fill;
        M3SpringAnimation* grow;
        // Working: the logo's beat
        QTimer beat;
        QElapsedTimer clock;
        double phase = -1;
        ClaudeUsage::Activity activity = ClaudeUsage::Idle;
};

namespace {
    constexpr int BeatMs = 1600;

    // The limits, and what stands in their place while Claude waits
    int limitsWidth(const QFont& font) {
        return 10 + 16 + 4 + 16 + 8 + QFontMetrics(label(font)).horizontalAdvance(figure(ClaudeUsage::instance()->session())) + 8;
    }
    int waitingWidth(const QFont& font) {
        return 8 + QFontMetrics(label(font)).horizontalAdvance(ClaudeChunk::tr("Needs you")) + 12;
    }
} // namespace

ClaudeChunk::ClaudeChunk() :
    Chunk() {
    d = new Private();
    this->setCursor(Qt::PointingHandCursor);
    connect(this, &Chunk::clicked, this, [] {
        StateManager::statusCenterManager()->showWithPane(QStringLiteral("OverviewPane"));
    });
    connect(ClaudeUsage::instance(), &ClaudeUsage::changed, this, &ClaudeChunk::follow);

    d->fill = new M3SpringAnimation(this);
    d->fill->setToken(M3Motion::EffectsDefault);
    d->fill->snapTo(0.0);
    connect(d->fill, &M3SpringAnimation::valueChanged, this, [this] {
        this->update();
    });
    d->grow = new M3SpringAnimation(this);
    d->grow->setToken(M3Motion::SpatialDefault);
    d->grow->snapTo(0.0);
    connect(d->grow, &M3SpringAnimation::valueChanged, this, [this] {
        this->updateGeometry();
        this->update();
    });

    // The beat ends where the logo is whole, so work that stops mid-beat
    // lets it finish first
    d->beat.setInterval(33);
    connect(&d->beat, &QTimer::timeout, this, [this] {
        const double phase = (d->clock.elapsed() % BeatMs) / double(BeatMs);
        if (phase < d->phase && d->activity != ClaudeUsage::Working) {
            d->beat.stop();
            d->phase = -1;
        } else {
            d->phase = phase;
        }
        this->update();
    });
}

ClaudeChunk::~ClaudeChunk() {
    delete d;
}

QString ClaudeChunk::name() {
    return QStringLiteral("claude");
}

int ClaudeChunk::expandedHeight() {
    return Height + 12;
}

int ClaudeChunk::statusBarHeight() {
    return Height;
}

QSize ClaudeChunk::sizeHint() const {
    const double grown = d->grow->currentValue().toDouble();
    const double content = limitsWidth(this->font()) + (waitingWidth(this->font()) - limitsWidth(this->font())) * grown;
    return QSize(6 + 18 + qMax(0, qRound(content)), Height);
}

void ClaudeChunk::follow() {
    ClaudeUsage* usage = ClaudeUsage::instance();
    const bool show = usage->available();
    if (show && !this->chunkRegistered()) StateManager::barManager()->addChunk(this);
    if (!show && this->chunkRegistered()) StateManager::barManager()->removeChunk(this);

    const ClaudeUsage::Activity activity = usage->activity();
    if (activity != d->activity) {
        d->activity = activity;
        const double waiting = activity == ClaudeUsage::Waiting ? 1.0 : 0.0;
        d->fill->animateTo(waiting);
        d->grow->animateTo(waiting);
        // The rays move while it works; calmer motion leaves the logo still
        if (activity == ClaudeUsage::Working && M3Motion::expressive() && !d->beat.isActive()) {
            d->clock.start();
            d->phase = 0;
            d->beat.start();
        }
    }

    const QString limits = tr("%1 of this session, %2 of this week · %3").arg(figure(usage->session()), figure(usage->week()), usage->updatedText().toLower());
    if (activity == ClaudeUsage::Waiting) {
        this->setToolTip(tr("Claude needs you: %1").arg(usage->waitingText()) + QStringLiteral("\n") + limits);
    } else if (activity == ClaudeUsage::Working) {
        this->setToolTip(tr("Claude is working · %1").arg(limits));
    } else {
        this->setToolTip(tr("Claude: %1").arg(limits));
    }
    this->updateGeometry();
    this->update();
}

void ClaudeChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    ClaudeUsage* usage = ClaudeUsage::instance();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double cy = this->height() / 2.0;
    const double waiting = qBound(0.0, d->fill->currentValue().toDouble(), 1.0);

    // Waiting on the person: the chunk fills
    if (waiting > 0) {
        QColor container = s.tertiaryContainer;
        container.setAlphaF(waiting);
        p.setPen(Qt::NoPen);
        p.setBrush(container);
        p.drawRoundedRect(QRectF(0, cy - Height / 2.0, this->width(), Height), Height / 2.0, Height / 2.0);
    }

    ClaudeUsage::paintLogo(&p, QRectF(6, cy - 9, 18, 18), d->phase);

    // A ring each: this session, this week
    if (waiting < 1) {
        p.setOpacity(1 - waiting);
        auto ring = [&](double x, const ClaudeUsage::Limit& limit, QColor colour) {
            const QRectF r(x - 7, cy - 7, 14, 14);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(s.outlineVariant, 3, Qt::SolidLine, Qt::RoundCap));
            p.drawEllipse(r);
            if (limit.percent > 0.5) {
                p.setPen(QPen(colourFor(s, limit, colour), 3, Qt::SolidLine, Qt::RoundCap));
                p.drawArc(r, 90 * 16, int(-360 * 16 * qMin(1.0, limit.percent / 100)));
            }
        };
        ring(6 + 18 + 10 + 8, usage->session(), s.primary);
        ring(6 + 18 + 10 + 16 + 4 + 8, usage->week(), s.tertiary);

        p.setFont(label(this->font()));
        p.setPen(s.onSurface);
        p.drawText(QRectF(6 + 18 + 10 + 16 + 4 + 16 + 8, 0, this->width(), this->height()), Qt::AlignVCenter | Qt::AlignLeft, figure(usage->session()));
    }

    // ...and says so, where the limits were
    if (waiting > 0) {
        p.setOpacity(waiting);
        p.setFont(label(this->font()));
        p.setPen(s.onTertiaryContainer);
        p.drawText(QRectF(6 + 18 + 8, 0, this->width(), this->height()), Qt::AlignVCenter | Qt::AlignLeft, tr("Needs you"));
    }
}
