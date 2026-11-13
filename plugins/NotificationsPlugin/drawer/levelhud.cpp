/****************************************
 *
 *   theDesk - the level pop-up (volume, brightness)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "levelhud.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <Wm/desktopwm.h>
#include <compositor.h>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    constexpr int PanelWidth = 96;
    constexpr int PanelHeight = 360;
    constexpr int EdgeMargin = 24;
    constexpr int Track = 56;      // M3E slider, size L
    constexpr int TrackLength = 270;
    constexpr double Gap = 6, Handle = 4, OuterCorner = 16, InnerCorner = 4;

    // A freedesktop icon name from the callers to a Material Symbol
    QString symbolFor(const QString& icon, bool muted) {
        if (muted) return QStringLiteral("volume_off");
        if (icon.contains(QStringLiteral("brightness")) || icon.contains(QStringLiteral("display"))) return QStringLiteral("light_mode");
        if (icon.contains(QStringLiteral("headphone"))) return QStringLiteral("headphones");
        if (icon.contains(QStringLiteral("bluetooth"))) return QStringLiteral("bluetooth");
        if (icon.contains(QStringLiteral("lineout"))) return QStringLiteral("speaker");
        if (icon.contains(QStringLiteral("keyboard"))) return QStringLiteral("keyboard");
        return QStringLiteral("volume_up");
    }

    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    // A track segment: big corners at the track's own end, small ones by the
    // handle (vertical: `from`/`to` measured up from the bottom)
    QPainterPath segment(const QRectF& track, double from, double to, bool bottomEnd) {
        QRectF r(track.left(), track.bottom() - to, track.width(), to - from);
        const double bottom = bottomEnd ? OuterCorner : InnerCorner;
        const double top = bottomEnd ? InnerCorner : OuterCorner;
        QPainterPath path;
        path.moveTo(r.left() + top, r.top());
        path.lineTo(r.right() - top, r.top());
        path.quadTo(r.right(), r.top(), r.right(), r.top() + top);
        path.lineTo(r.right(), r.bottom() - bottom);
        path.quadTo(r.right(), r.bottom(), r.right() - bottom, r.bottom());
        path.lineTo(r.left() + bottom, r.bottom());
        path.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - bottom);
        path.lineTo(r.left(), r.top() + top);
        path.quadTo(r.left(), r.top(), r.left() + top, r.top());
        path.closeSubpath();
        return path;
    }
} // namespace

struct LevelHudPrivate {
        M3SpringAnimation* shown;
        M3SpringAnimation* level;
        QTimer* hide;
        QString symbol;
        bool muted = false;
        double target = 0;
        bool hiding = false;
};

LevelHud::LevelHud(QWidget* parent) :
    QWidget(parent) {
    d = new LevelHudPrivate();
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setWindowTitle(QStringLiteral("theDesk level"));
    this->setFixedSize(PanelWidth + EdgeMargin + 16, PanelHeight + 32); // room to slide and for the shadow

    d->shown = new M3SpringAnimation(this);
    d->shown->setToken(M3Motion::SpatialDefault);
    d->shown->snapTo(0.0);
    connect(d->shown, &M3SpringAnimation::valueChanged, this, [this] {
        if (d->hiding && d->shown->currentValue().toDouble() <= 0.02) {
            d->hiding = false;
            this->hide();
        }
        this->update();
    });
    d->level = new M3SpringAnimation(this);
    d->level->setToken(M3Motion::SpatialFast);
    d->level->snapTo(0.0);
    connect(d->level, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));

    d->hide = new QTimer(this);
    d->hide->setSingleShot(true);
    connect(d->hide, &QTimer::timeout, this, [this] {
        d->hiding = true;
        d->shown->setToken(M3Motion::EffectsDefault);
        d->shown->animateTo(0.0);
    });
}

LevelHud::~LevelHud() {
    delete d;
}

void LevelHud::showLevel(const QVariantMap& parameters) {
    const QString icon = parameters.value(QStringLiteral("icon")).toString();
    d->muted = icon.contains(QStringLiteral("muted"));
    d->symbol = symbolFor(icon, d->muted);
    d->target = parameters.value(QStringLiteral("value")).toDouble();

    if (!this->isVisible() || d->hiding) {
        // From where it is, or from the right value if it was gone
        if (!this->isVisible()) d->level->snapTo(d->target);
        d->hiding = false;
        place();
        this->show();
        this->raise();
        DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeNotification);
        d->shown->setToken(M3Motion::SpatialDefault);
        d->shown->animateTo(1.0);
    }
    d->level->animateTo(d->target);
    d->hide->start(parameters.value(QStringLiteral("timeout"), 1800).toInt());
}

void LevelHud::place() {
    QScreen* screen = qApp->primaryScreen();
    if (!screen) return;
    const QRect area = screen->geometry();
    this->move(area.right() - this->width() + 1, area.center().y() - this->height() / 2);

    // No compositor: the window is the panel's shape
    if (!Compositor::isActive()) {
        QPainterPath panel;
        panel.addRoundedRect(QRectF(this->width() - EdgeMargin - PanelWidth, 16, PanelWidth, PanelHeight), 28, 28);
        this->setMask(QRegion(panel.toFillPolygon().toPolygon()));
    } else {
        this->clearMask();
    }
}

void LevelHud::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    const double shown = d->shown->currentValue().toDouble();
    if (shown <= 0.01) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const bool composited = Compositor::isActive();
    if (composited) p.setOpacity(qBound(0.0, shown * 1.5, 1.0));

    // In from the edge (without a compositor the mask holds it in place)
    const double slide = composited ? (1 - qBound(0.0, shown, 1.2)) * 60 : 0;
    const QRectF panel(this->width() - EdgeMargin - PanelWidth + slide, 16, PanelWidth, PanelHeight);
    if (composited) {
        for (int i = 6; i >= 1; i--) {
            QPainterPath shadow;
            shadow.addRoundedRect(panel.adjusted(-i, -i + 3, i, i + 3), 28 + i, 28 + i);
            p.fillPath(shadow, QColor(0, 0, 0, 10 / i));
        }
    }
    QPainterPath container;
    container.addRoundedRect(panel, 28, 28);
    p.fillPath(container, s.surfaceContainerHigh);

    // The level, above
    const double level = d->level->currentValue().toDouble();
    const bool boosted = d->target > 1.0;
    p.setPen(boosted ? s.tertiary : s.onSurface);
    p.setFont(d->muted ? flex(16, 750) : flex(26, 750, 112));
    p.drawText(QRectF(panel.left(), panel.top() + 14, panel.width(), 40), Qt::AlignCenter, d->muted ? tr("Muted") : QLocale().toString(qRound(d->target * 100)));

    // The slider
    const QRectF track(panel.center().x() - Track / 2.0, panel.top() + 66, Track, TrackLength);
    const double usable = TrackLength - Handle - 2 * Gap;
    const double pos = qBound(0.0, level, 1.0) * usable;
    const QColor active = d->muted ? s.outline : boosted ? s.tertiary : s.primary;
    if (pos > 1) p.fillPath(segment(track, 0, pos, true), active);
    if (pos + 2 * Gap + Handle < TrackLength - 1) p.fillPath(segment(track, pos + 2 * Gap + Handle, TrackLength, false), s.secondaryContainer);
    QPainterPath handle;
    handle.addRoundedRect(QRectF(track.left() - 6, track.bottom() - pos - Gap - Handle, Track + 12, Handle), 2, 2);
    p.fillPath(handle, active);

    // The symbol: inside the active track, or in the inactive one when the
    // level is too low to hold it
    const bool inside = pos > 44;
    const QPointF centre(track.center().x(), inside ? track.bottom() - 24 : track.bottom() - pos - 2 * Gap - Handle - 24);
    QFont symbol(QStringLiteral("Material Symbols Rounded"));
    symbol.setPixelSize(24);
    symbol.setVariableAxis(QFont::Tag("FILL"), 1);
    symbol.setVariableAxis(QFont::Tag("wght"), 500);
    symbol.setVariableAxis(QFont::Tag("opsz"), 24);
    p.setFont(symbol);
    p.setPen(inside ? (d->muted ? s.surface : boosted ? s.onTertiary : s.onPrimary) : s.onSecondaryContainer);
    p.drawText(QRectF(centre.x() - 20, centre.y() - 20, 40, 40), Qt::AlignCenter, d->symbol);
}
