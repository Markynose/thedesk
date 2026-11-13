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
#include "statuscenterquickswitch.h"
#include "ui_statuscenterquickswitch.h"

#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <m3motion.h>
#include <m3scheme.h>
#include <quickswitch.h>

// M3 Expressive quick-setting tile, in the manner of Android 16's: an inactive
// tile is a SurfaceContainerHighest pill; switching it on fills it with Primary
// and springs its corners tighter; pressing squishes the corners further.
// Icon, title, and the state underneath.

namespace {
    constexpr int tileHeight = 64;
    constexpr int tileWidth = 176;
    constexpr double activeCorner = 20;
    constexpr double pressedCorner = 12;

    QString iconFor(const QString& name) {
        static const QHash<QString, QStringList> icons = {
            {QStringLiteral("NetworkFlight"),    {QStringLiteral("airplane-mode-symbolic")}                                                             },
            {QStringLiteral("NetworkWireless"),  {QStringLiteral("network-wireless-symbolic")}                                                          },
            {QStringLiteral("NetworkCellular"),  {QStringLiteral("network-cellular-symbolic")}                                                          },
            {QStringLiteral("Bluetooth"),        {QStringLiteral("bluetooth-active-symbolic"), QStringLiteral("bluetooth-symbolic")}                    },
            {QStringLiteral("PowerStretch"),     {QStringLiteral("power-profile-power-saver-symbolic"), QStringLiteral("battery-profile-powersave-symbolic")}},
            {QStringLiteral("Redshift"),         {QStringLiteral("night-light-symbolic")}                                                               },
            {QStringLiteral("PowerCaffeinate"),  {QStringLiteral("my-caffeine-on-symbolic"), QStringLiteral("caffeine-cup-full")}                       },
        };
        for (const QString& icon : icons.value(name)) {
            if (QIcon::hasThemeIcon(icon)) return icon;
        }
        return QStringLiteral("preferences-system-symbolic");
    }

    // Symbolic icons take the tile's content colour
    QPixmap tinted(const QIcon& icon, int size, qreal dpr, QColor colour) {
        QPixmap pixmap = icon.pixmap(QSize(size, size), dpr);
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pixmap.rect(), colour);
        return pixmap;
    }

    QColor mix(QColor a, QColor b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    }
} // namespace

struct StatusCenterQuickSwitchPrivate {
    QuickSwitch* sw;
    QIcon icon;
    M3SpringAnimation* active = nullptr; // 0 off -> 1 on: corners (spatial, bouncy)
    M3SpringAnimation* colour = nullptr; // 0 off -> 1 on: colours (effects, quick)
    M3SpringAnimation* press = nullptr;  // 0 -> 1: squish
    M3SpringAnimation* hover = nullptr;  // 0 -> 1: state layer
    bool keyboardFocus = false;
};

StatusCenterQuickSwitch::StatusCenterQuickSwitch(QuickSwitch* sw, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::StatusCenterQuickSwitch) {
    ui->setupUi(this);
    d = new StatusCenterQuickSwitchPrivate();

    d->sw = sw;
    ui->sw->setChecked(sw->isChecked());
    ui->titleLabel->setText(sw->title());

    sw->setSwitch(ui->sw);
    connect(sw, &QuickSwitch::titleChanged, ui->titleLabel, &QLabel::setText);
    connect(sw, &QuickSwitch::titleChanged, this, qOverload<>(&QWidget::update));

    // The tile is the control now; the switch stays as the QuickSwitch's model
    ui->sw->setVisible(false);
    ui->titleLabel->setVisible(false);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setAttribute(Qt::WA_Hover);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    this->setFixedHeight(tileHeight);
    d->icon = QIcon::fromTheme(iconFor(sw->name()));

    d->active = new M3SpringAnimation(this);
    d->active->setToken(M3Motion::SpatialDefault);
    d->active->snapTo(sw->isChecked() ? 1.0 : 0.0);
    d->colour = new M3SpringAnimation(this);
    d->colour->setToken(M3Motion::EffectsFast);
    d->colour->snapTo(sw->isChecked() ? 1.0 : 0.0);
    d->press = new M3SpringAnimation(this);
    d->press->setToken(M3Motion::SpatialFast);
    d->press->snapTo(0.0);
    d->hover = new M3SpringAnimation(this);
    d->hover->setToken(M3Motion::EffectsFast);
    d->hover->snapTo(0.0);
    for (auto* spring : {d->active, d->colour, d->press, d->hover}) connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

QSize StatusCenterQuickSwitch::sizeHint() const {
    return QSize(tileWidth, tileHeight);
}

void StatusCenterQuickSwitch::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    M3::Scheme s = M3::currentScheme();
    const double on = d->active->currentValue().toDouble();
    const double press = d->press->currentValue().toDouble();

    // Pill when off, CornerLarge-ish when on, tighter still while pressed
    QRectF r = QRectF(this->rect()).adjusted(2, 2, -2, -2);
    double corner = r.height() / 2 + (activeCorner - r.height() / 2) * qBound(-0.2, on, 1.2);
    corner += (pressedCorner - corner) * press;
    corner = qBound(4.0, corner, r.height() / 2);
    QPainterPath tile;
    tile.addRoundedRect(r, corner, corner);

    // Colours change fast (effects spring) so the label never sinks into a
    // half-way container; the shape takes the bouncy spatial spring
    const double tint = d->colour->currentValue().toDouble();
    QColor container = mix(s.surfaceContainerHighest, s.primary, tint);
    QColor content = tint < 0.5 ? s.onSurface : s.onPrimary;
    QColor secondary = tint < 0.5 ? s.onSurfaceVariant : s.onPrimary;
    painter.fillPath(tile, container);
    QColor layer = content;
    layer.setAlphaF(0.08 * d->hover->currentValue().toDouble() + 0.10 * press);
    painter.fillPath(tile, layer);
    if (this->hasFocus() && d->keyboardFocus) {
        // Keyboard focus: a 2 dp Secondary ring just inside the tile
        painter.setPen(QPen(s.secondary, 2));
        painter.drawPath(tile);
    }

    // Icon, then the title over its state
    const int iconSize = 20;
    QRectF iconRect(r.left() + 18, r.center().y() - iconSize / 2.0, iconSize, iconSize);
    painter.drawPixmap(iconRect.topLeft(), tinted(d->icon, iconSize, this->devicePixelRatioF(), content));

    QRectF textRect(iconRect.right() + 12, r.top(), r.right() - iconRect.right() - 12 - 14, r.height());
    QFont title = this->font();
    title.setPixelSize(14); // TitleSmall
    title.setWeight(QFont::Medium);
    QFont state = this->font();
    state.setPixelSize(12); // BodySmall
    QFontMetricsF tm(title), sm(state);
    double top = r.center().y() - (tm.height() + sm.height()) / 2;
    painter.setFont(title);
    painter.setPen(content);
    painter.drawText(QRectF(textRect.left(), top, textRect.width(), tm.height()), Qt::AlignLeading | Qt::AlignVCenter, tm.elidedText(d->sw->title(), Qt::ElideRight, textRect.width()));
    painter.setFont(state);
    painter.setPen(secondary);
    painter.drawText(QRectF(textRect.left(), top + tm.height(), textRect.width(), sm.height()), Qt::AlignLeading | Qt::AlignVCenter, d->sw->isChecked() ? tr("On") : tr("Off"));
}

void StatusCenterQuickSwitch::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return QWidget::mousePressEvent(event);
    d->press->animateTo(1.0);
}

void StatusCenterQuickSwitch::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return QWidget::mouseReleaseEvent(event);
    d->press->animateTo(0.0);
    if (this->rect().contains(event->position().toPoint())) ui->sw->toggle();
}

void StatusCenterQuickSwitch::enterEvent(QEnterEvent* event) {
    d->hover->animateTo(1.0);
    QWidget::enterEvent(event);
}

void StatusCenterQuickSwitch::leaveEvent(QEvent* event) {
    d->hover->animateTo(0.0);
    d->press->animateTo(0.0);
    QWidget::leaveEvent(event);
}

void StatusCenterQuickSwitch::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        ui->sw->toggle();
        return;
    }
    QWidget::keyPressEvent(event);
}

void StatusCenterQuickSwitch::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange) this->update();
    QWidget::changeEvent(event);
}

void StatusCenterQuickSwitch::focusInEvent(QFocusEvent* event) {
    d->keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    this->update();
    QWidget::focusInEvent(event);
}

void StatusCenterQuickSwitch::focusOutEvent(QFocusEvent* event) {
    d->keyboardFocus = false;
    this->update();
    QWidget::focusOutEvent(event);
}

StatusCenterQuickSwitch::~StatusCenterQuickSwitch() {
    delete d;
    delete ui;
}

void StatusCenterQuickSwitch::on_sw_toggled(bool checked) {
    d->sw->setChecked(checked);
    if (d->active) d->active->animateTo(checked ? 1.0 : 0.0); // not yet built during setupUi
    if (d->colour) d->colour->animateTo(checked ? 1.0 : 0.0);

    this->update();
}
