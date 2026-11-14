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
#include <QHash>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <m3motion.h>
#include <m3scheme.h>
#include <quickswitch.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>

// M3 Expressive quick-setting tiles, in the manner of Android 16's. Off, a
// tile is a SurfaceContainerHighest pill; on, it fills with Primary and springs
// its corners tighter; pressing squishes them further. A switch with a detail
// is a big tile: its icon sits in its own shape (a circle off, a square on)
// and toggles, the rest opens the detail. The others are small round tiles.

namespace {
    constexpr int tileHeight = 64;
    constexpr int bigTileWidth = 200;
    constexpr double activeCorner = 20;
    constexpr double pressedCorner = 12;

    QIcon iconFor(const QString& name, bool on) {
        static const QHash<QString, QStringList> icons = {
            {QStringLiteral("NetworkFlight"),      {QStringLiteral("airplane-mode-symbolic")}                                                                 },
            {QStringLiteral("NetworkWireless"),    {QStringLiteral("network-wireless-symbolic")}                                                              },
            {QStringLiteral("NetworkCellular"),    {QStringLiteral("network-cellular-symbolic")}                                                              },
            {QStringLiteral("Bluetooth"),          {QStringLiteral("bluetooth-active-symbolic"), QStringLiteral("bluetooth-symbolic")}                        },
            {QStringLiteral("NotificationsQuiet"), {QStringLiteral("do-not-disturb-on-symbolic"), QStringLiteral("quiet-mode-symbolic")}                      },
            {QStringLiteral("PowerStretch"),       {QStringLiteral("power-profile-power-saver-symbolic"), QStringLiteral("battery-profile-powersave-symbolic")}},
            {QStringLiteral("Redshift"),           {QStringLiteral("night-light-symbolic")}                                                                   },
            {QStringLiteral("PowerCaffeinate"),    {QStringLiteral("my-caffeine-on-symbolic"), QStringLiteral("caffeine-cup-full")}                           },
        };
        // the big tiles say "off" with a symbol of its own
        static const QHash<QString, QString> offIcons = {
            {QStringLiteral("NetworkWireless"),    QStringLiteral("network-wireless-offline-symbolic")},
            {QStringLiteral("Bluetooth"),          QStringLiteral("bluetooth-disabled-symbolic")      },
            {QStringLiteral("NotificationsQuiet"), QStringLiteral("do-not-disturb-off-symbolic")      },
        };
        QStringList names = icons.value(name);
        if (!on && offIcons.contains(name)) names.prepend(offIcons.value(name));
        for (const QString& icon : names) {
            if (QIcon::hasThemeIcon(icon)) return QIcon::fromTheme(icon);
        }
        return QIcon::fromTheme(QStringLiteral("preferences-system-symbolic"));
    }

    // Symbolic icons take the tile's content colour (filled when on)
    QPixmap tinted(const QIcon& icon, int size, qreal dpr, QColor colour, bool filled = false) {
        // made once per look: every tile repaints on each frame of the shade opening
        static QHash<QString, QPixmap> made;
        const QString key = QStringLiteral("%1 %2 %3 %4 %5").arg(icon.name()).arg(size).arg(dpr).arg(colour.rgba()).arg(filled);
        const bool named = !icon.name().isEmpty(); // a theme icon: known by its name
        if (named && made.contains(key)) return made.value(key);
        QPixmap pixmap = icon.pixmap(QSize(size, size), dpr, QIcon::Normal, filled ? QIcon::On : QIcon::Off);
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pixmap.rect(), colour);
        p.end();
        if (made.size() > 200) made.clear();
        if (named) made.insert(key, pixmap);
        return pixmap;
    }

    QColor mix(QColor a, QColor b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    }

    QColor withAlpha(QColor c, double a) {
        c.setAlphaF(float(qBound(0.0, a, 1.0)));
        return c;
    }
} // namespace

struct StatusCenterQuickSwitchPrivate {
    QuickSwitch* sw;
    bool big = false;
    bool pressOnIcon = false;
    M3SpringAnimation* active = nullptr;   // 0 off -> 1 on: corners, icon shape (spatial, bouncy)
    M3SpringAnimation* colour = nullptr;   // 0 off -> 1 on: colours (effects, quick)
    M3SpringAnimation* press = nullptr;    // 0 -> 1: squish
    M3SpringAnimation* hover = nullptr;    // 0 -> 1: state layer
    M3SpringAnimation* entrance = nullptr; // 0 -> 1: dropping in as the shade opens
    bool keyboardFocus = false;
};

StatusCenterQuickSwitch::StatusCenterQuickSwitch(QuickSwitch* sw, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::StatusCenterQuickSwitch) {
    ui->setupUi(this);
    d = new StatusCenterQuickSwitchPrivate();

    d->sw = sw;
    d->big = sw->detail() || !sw->detailPane().isEmpty();
    ui->sw->setChecked(sw->isChecked());
    ui->titleLabel->setText(sw->title());

    sw->setSwitch(ui->sw);
    connect(sw, &QuickSwitch::titleChanged, ui->titleLabel, &QLabel::setText);
    connect(sw, &QuickSwitch::titleChanged, this, &StatusCenterQuickSwitch::updateLabels);

    // The tile is the control now; the switch stays as the QuickSwitch's model
    ui->sw->setVisible(false);
    ui->titleLabel->setVisible(false);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setAttribute(Qt::WA_Hover);
    if (d->big) {
        this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        this->setFixedHeight(tileHeight);
    } else {
        this->setFixedSize(tileHeight, tileHeight);
    }
    updateLabels();

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
    d->entrance = new M3SpringAnimation(this);
    d->entrance->setToken(M3Motion::SpatialDefault);
    d->entrance->snapTo(1.0);
    for (auto* spring : {d->active, d->colour, d->press, d->hover, d->entrance}) connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

QSize StatusCenterQuickSwitch::sizeHint() const {
    return d->big ? QSize(bigTileWidth, tileHeight) : QSize(tileHeight, tileHeight);
}

QSize StatusCenterQuickSwitch::minimumSizeHint() const {
    return d->big ? QSize(160, tileHeight) : QSize(tileHeight, tileHeight);
}

bool StatusCenterQuickSwitch::isBig() const {
    return d->big;
}

QuickSwitch* StatusCenterQuickSwitch::quickSwitch() const {
    return d->sw;
}

void StatusCenterQuickSwitch::playEntrance(int delay) {
    d->entrance->snapTo(0.0);
    QTimer::singleShot(delay, this, [this] {
        d->entrance->animateTo(1.0);
    });
}

void StatusCenterQuickSwitch::updateLabels() {
    const QString state = d->sw->isChecked() ? tr("On") : tr("Off");
    this->setAccessibleName(d->sw->title());
    this->setAccessibleDescription(state);
    // small tiles are just a symbol: the name comes up as a tooltip
    this->setToolTip(d->big ? QString() : QStringLiteral("%1 · %2").arg(d->sw->title(), state));
    this->update();
}

QRectF StatusCenterQuickSwitch::iconBounds() const {
    const QRectF r = QRectF(this->rect()).adjusted(1, 1, -1, -1);
    const double size = r.height() - 16;
    const bool rtl = this->layoutDirection() == Qt::RightToLeft;
    return QRectF(rtl ? r.right() - 8 - size : r.left() + 8, r.top() + 8, size, size);
}

void StatusCenterQuickSwitch::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    M3::Scheme s = M3::currentScheme();
    const double on = d->active->currentValue().toDouble();
    const double press = d->press->currentValue().toDouble();
    const bool checked = d->sw->isChecked();
    const bool rtl = this->layoutDirection() == Qt::RightToLeft;

    // Dropping in: fades up and grows to size
    const double entrance = d->entrance->currentValue().toDouble();
    if (entrance < 0.999) {
        const double scale = 0.85 + 0.15 * qMin(entrance, 1.1);
        painter.setOpacity(qBound(0.0, entrance, 1.0));
        painter.translate(this->width() / 2.0, this->height() / 2.0);
        painter.scale(scale, scale);
        painter.translate(-this->width() / 2.0, -this->height() / 2.0);
    }

    // Pill (or circle) when off, CornerLarge-ish when on, tighter still while pressed
    QRectF r = QRectF(this->rect()).adjusted(1, 1, -1, -1);
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
    painter.fillPath(tile, withAlpha(content, 0.08 * d->hover->currentValue().toDouble() + 0.10 * press));
    if (this->hasFocus() && d->keyboardFocus) {
        // Keyboard focus: a 2 dp Secondary ring just inside the tile
        painter.setPen(QPen(s.secondary, 2));
        painter.drawPath(tile);
        painter.setPen(Qt::NoPen);
    }

    const QIcon icon = iconFor(d->sw->name(), checked);
    if (!d->big) {
        const int iconSize = 24;
        painter.drawPixmap(QPointF(r.center().x() - iconSize / 2.0, r.center().y() - iconSize / 2.0), tinted(icon, iconSize, this->devicePixelRatioF(), tint < 0.5 ? s.onSurfaceVariant : s.onPrimary, checked));
        return;
    }

    // The icon's own shape: a circle off, morphing to a square on
    static const m3shapes::Morph iconMorph(MaterialShapes::circle(), MaterialShapes::square());
    const QRectF ib = iconBounds();
    painter.fillPath(m3shapes::toPath(iconMorph, float(qBound(0.0, on, 1.0)), ib), tint < 0.5 ? withAlpha(s.onSurface, 0.08) : withAlpha(s.onPrimary, 0.16 * tint));
    const int iconSize = 22;
    painter.drawPixmap(QPointF(ib.center().x() - iconSize / 2.0, ib.center().y() - iconSize / 2.0), tinted(icon, iconSize, this->devicePixelRatioF(), content, checked));

    // Title over its state, then a chevron: this part opens the detail
    const int chevron = 20;
    QRectF textRect = rtl ? QRectF(r.left() + 36, r.top(), ib.left() - 12 - r.left() - 36, r.height())
                          : QRectF(ib.right() + 12, r.top(), r.right() - 36 - ib.right() - 12, r.height());
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
    painter.drawText(QRectF(textRect.left(), top + tm.height(), textRect.width(), sm.height()), Qt::AlignLeading | Qt::AlignVCenter, checked ? tr("On") : tr("Off"));
    const QIcon chevronIcon = QIcon::fromTheme(rtl ? QStringLiteral("pan-start-symbolic") : QStringLiteral("pan-end-symbolic"));
    painter.drawPixmap(QPointF(rtl ? r.left() + 12 : r.right() - 12 - chevron, r.center().y() - chevron / 2.0), tinted(chevronIcon, chevron, this->devicePixelRatioF(), secondary));
}

void StatusCenterQuickSwitch::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return QWidget::mousePressEvent(event);
    // A big tile has two targets: the icon toggles, the rest opens the detail
    d->pressOnIcon = !d->big || iconBounds().adjusted(-4, -8, 4, 8).contains(event->position());
    d->press->animateTo(1.0);
}

void StatusCenterQuickSwitch::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return QWidget::mouseReleaseEvent(event);
    d->press->animateTo(0.0);
    if (!this->rect().contains(event->position().toPoint())) return;
    if (d->pressOnIcon) ui->sw->toggle();
    else emit detailRequested();
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
    // Space toggles; Enter opens a big tile's detail (it toggles a small one)
    if (event->key() == Qt::Key_Space || (!d->big && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))) {
        ui->sw->toggle();
        return;
    }
    if (d->big && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit detailRequested();
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
    if (d->entrance) updateLabels();

    this->update();
}
