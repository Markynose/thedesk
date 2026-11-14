/****************************************
 *
 *   theDesk - a volume slider in the sound panel
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "volumecolumn.h"

#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    constexpr int ColumnWidth = 88;
    constexpr double NumberHeight = 30, TrackTop = 38, NameArea = 48; // name, an "on <device>" line, gaps
    constexpr double Gap = 6, OuterCorner = 16, InnerCorner = 2;

    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    QFont symbolFont(double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        return font;
    }

    // A rect with its own radius per corner
    QPainterPath corners(const QRectF& r, double tl, double tr, double br, double bl) {
        const double m = qMin(r.width(), r.height()) / 2;
        tl = qMin(tl, m), tr = qMin(tr, m), br = qMin(br, m), bl = qMin(bl, m);
        QPainterPath path;
        path.moveTo(r.left() + tl, r.top());
        path.arcTo(QRectF(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr), 90, -90);
        path.arcTo(QRectF(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br), 0, -90);
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl), 270, -90);
        path.arcTo(QRectF(r.left(), r.top(), 2 * tl, 2 * tl), 180, -90);
        path.closeSubpath();
        return path;
    }
} // namespace

VolumeColumn::VolumeColumn(bool master, QWidget* parent) :
    QAbstractSlider(parent), master(master) {
    this->setOrientation(Qt::Vertical);
    this->setRange(0, 100);
    this->setSingleStep(5);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    level = new M3SpringAnimation(this);
    level->setToken(M3Motion::SpatialFast);
    level->snapTo(0.0);
    connect(level, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

void VolumeColumn::setSymbol(const QString& symbol) {
    this->symbol = symbol;
    this->update();
}

void VolumeColumn::setName(const QString& name) {
    this->name = name;
    this->setAccessibleName(name);
    this->update();
}

void VolumeColumn::setSubtitle(const QString& subtitle) {
    this->subtitle = subtitle;
    this->update();
}

void VolumeColumn::setShownLevel(int percent) {
    shown = percent;
    this->update();
}

void VolumeColumn::setMuted(bool muted) {
    this->muted = muted;
    this->update();
}

void VolumeColumn::setMenu(QMenu* menu) {
    this->menu = menu;
}

QSize VolumeColumn::sizeHint() const {
    return QSize(ColumnWidth, int(TrackTop + 260 + NameArea));
}

QSize VolumeColumn::minimumSizeHint() const {
    return QSize(ColumnWidth, int(TrackTop + 120 + NameArea));
}

QRectF VolumeColumn::track() const {
    const double w = master ? 52 : 40;
    return QRectF(this->width() / 2.0 - w / 2, TrackTop, w, this->height() - TrackTop - NameArea);
}

QRectF VolumeColumn::nameRect() const {
    return QRectF(0, track().bottom() + 10, this->width(), NameArea - 10);
}

QRectF VolumeColumn::iconRect() const {
    // Inside the active track, or above the handle when the level is too low to hold it
    const QRectF t = track();
    const double handle = 4, fill = qBound(0.0, level->currentValue().toDouble() / 100, 1.0) * (t.height() - handle) - Gap;
    const double y = fill > 44 ? t.bottom() - 22 : t.bottom() - fill - 2 * Gap - handle - 22;
    return QRectF(t.center().x() - 20, y - 20, 40, 40);
}

int VolumeColumn::valueAt(double y) const {
    const QRectF t = track();
    return qBound(0, qRound((t.bottom() - 2 - y) / (t.height() - 4) * 100), 100);
}

void VolumeColumn::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const bool enabled = this->isEnabled();
    const int percent = shown >= 0 ? shown : this->value();
    const bool boosted = percent > 100;

    // The level, above
    if (muted) {
        p.setFont(flex(16, 700));
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(0, 0, this->width(), NumberHeight), Qt::AlignCenter, tr("Muted"));
    } else {
        p.setFont(master ? flex(24, 760, 112) : flex(18, 640, 112));
        p.setPen(boosted ? s.tertiary : master ? s.onSurface : s.onSurfaceVariant);
        p.drawText(QRectF(0, 0, this->width(), NumberHeight), Qt::AlignCenter, enabled ? QLocale().toString(percent) : QStringLiteral("–"));
    }

    // The slider: active track, gap, handle, gap, inactive track, stop dot
    const QRectF t = track();
    const double handle = this->isSliderDown() || this->hasFocus() ? 2 : 4;
    const double cy = t.bottom() - 2 - qBound(0.0, level->currentValue().toDouble() / 100, 1.0) * (t.height() - 4);
    QColor active = muted ? s.outline : boosted ? s.tertiary : s.primary;
    QColor inactive = s.secondaryContainer;
    if (!enabled) {
        active = s.onSurface;
        active.setAlphaF(0.38f);
        inactive = s.onSurface;
        inactive.setAlphaF(0.12f);
    }
    const double activeTop = cy + handle / 2 + Gap, inactiveBottom = cy - handle / 2 - Gap;
    if (t.bottom() - activeTop > 1) p.fillPath(corners(QRectF(t.left(), activeTop, t.width(), t.bottom() - activeTop), InnerCorner, InnerCorner, OuterCorner, OuterCorner), active);
    if (inactiveBottom - t.top() > 1) {
        p.fillPath(corners(QRectF(t.left(), t.top(), t.width(), inactiveBottom - t.top()), OuterCorner, OuterCorner, InnerCorner, InnerCorner), inactive);
        if (inactiveBottom - t.top() > t.width()) {
            p.setPen(Qt::NoPen);
            p.setBrush(active);
            p.drawEllipse(QPointF(t.center().x(), t.top() + 12), 2, 2);
        }
    }
    QPainterPath bar;
    bar.addRoundedRect(QRectF(t.left() - 6, cy - handle / 2, t.width() + 12, handle), handle / 2, handle / 2);
    p.fillPath(bar, active);

    // The symbol (the master's mutes)
    const QRectF icon = iconRect();
    const bool inside = icon.bottom() > cy;
    p.setFont(symbolFont(master ? 24 : 20));
    p.setPen(!inside ? s.onSecondaryContainer : muted ? s.surface : boosted ? s.onTertiary : s.onPrimary);
    p.drawText(icon, Qt::AlignCenter, muted ? QStringLiteral("volume_off") : symbol);

    // The name, with the menu's arrow for apps, and where it plays
    const QRectF n = nameRect();
    const QFont nameFont = flex(13, 560);
    const double arrow = menu ? 16 : 0;
    const QString elided = QFontMetricsF(nameFont).elidedText(name, Qt::ElideRight, this->width() - arrow - 4);
    const double nw = QFontMetricsF(nameFont).horizontalAdvance(elided) + arrow;
    p.setFont(nameFont);
    p.setPen(s.onSurface);
    p.drawText(QRectF(this->width() / 2.0 - nw / 2, n.top(), nw, 22), Qt::AlignLeft | Qt::AlignVCenter, elided);
    if (menu) {
        p.setFont(symbolFont(18));
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(this->width() / 2.0 + nw / 2 - 18, n.top() + 2, 18, 18), Qt::AlignCenter, QStringLiteral("arrow_drop_down"));
    }
    if (!subtitle.isEmpty()) {
        const QFont subFont = flex(12, 450);
        p.setFont(subFont);
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(0, n.top() + 22, this->width(), 16), Qt::AlignCenter, QFontMetricsF(subFont).elidedText(subtitle, Qt::ElideRight, this->width() - 4));
    }
}

void VolumeColumn::mousePressEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (master && iconRect().contains(pos)) {
        iconPressed = true;
    } else if (track().adjusted(-8, -8, 8, 8).contains(pos)) {
        this->setSliderDown(true);
        this->setValue(valueAt(pos.y()));
    } else if (menu && nameRect().contains(pos)) {
        openMenu();
    }
}

void VolumeColumn::mouseMoveEvent(QMouseEvent* event) {
    if (this->isSliderDown()) this->setValue(valueAt(event->position().y()));
}

void VolumeColumn::mouseReleaseEvent(QMouseEvent* event) {
    if (iconPressed && iconRect().contains(event->position())) emit iconClicked();
    iconPressed = false;
    if (this->isSliderDown()) this->setSliderDown(false);
    this->update();
}

void VolumeColumn::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Menu:
            if (master) emit iconClicked();
            else openMenu();
            break;
        default:
            QAbstractSlider::keyPressEvent(event);
    }
}

void VolumeColumn::sliderChange(SliderChange change) {
    if (change == SliderValueChange) {
        // Dragging follows the pointer; the rest springs there
        if (this->isSliderDown()) level->snapTo(double(this->value()));
        else level->animateTo(double(this->value()));
    }
    QAbstractSlider::sliderChange(change);
}

void VolumeColumn::openMenu() {
    if (menu) menu->popup(this->mapToGlobal(nameRect().bottomLeft().toPoint()));
}
