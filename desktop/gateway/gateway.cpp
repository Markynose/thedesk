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
#include "gateway.h"

#include "launcher.h"
#include <Gestures/gesturedaemon.h>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QVariantAnimation>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include <Wm/desktopwm.h>
#include <compositor.h>
#include <gatewaymanager.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <statemanager.h>

namespace {
    // Geometry of the primary screen. ScreenDaemon can report no primary
    // screen (e.g. no RandR primary output), so fall back to Qt's.
    QRect primaryScreenGeometry() {
        if (auto* screen = ScreenDaemon::instance()->primayScreen()) return screen->geometry();
        if (auto* screen = qGuiApp->primaryScreen()) return screen->geometry();
        return {};
    }

    constexpr double scrimOpacity = 0.45; // over the desktop, under the launcher
} // namespace

struct GatewayPrivate {
        Gateway* instance = nullptr;
        Launcher* launcher = nullptr;
        M3SpringAnimation* scrim = nullptr; // an effect: it never overshoots
        QVariantAnimation* leave = nullptr; // the window fades away
        bool open = false;
};

GatewayPrivate* Gateway::d = new GatewayPrivate();

Gateway::Gateway() :
    QDialog(nullptr) {
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeMenu);
    this->setWindowTitle(QStringLiteral("theDesk Launcher"));

    d->launcher = new Launcher(this);
    connect(d->launcher, &Launcher::closeRequested, this, [this] {
        this->close();
    });
    connect(d->launcher, &Launcher::shapeChanged, this, &Gateway::applyShape);
    connect(Compositor::instance(), &Compositor::activeChanged, this, [this] {
        if (this->isVisible()) place();
    });

    d->scrim = new M3SpringAnimation(this);
    d->scrim->setToken(M3Motion::EffectsDefault);
    d->scrim->snapTo(0.0);
    connect(d->scrim, &M3SpringAnimation::valueChanged, this, [this] {
        this->update();
    });

    d->leave = new QVariantAnimation(this);
    d->leave->setStartValue(1.0);
    d->leave->setEndValue(0.0);
    d->leave->setDuration(120);
    connect(d->leave, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        this->setWindowOpacity(value.toDouble());
    });
    connect(d->leave, &QVariantAnimation::finished, this, [this] {
        if (d->open) return; // opened again meanwhile
        QDialog::hide();
        this->setWindowOpacity(1);
    });

    // Three fingers sideways still open and close it
    connect(GestureDaemon::instance(), &GestureDaemon::gestureBegin, this, [this](GestureInteractionPtr interaction) {
        const bool rtl = this->layoutDirection() == Qt::RightToLeft;
        const bool opens = interaction->isValidInteraction(GestureTypes::Swipe, rtl ? GestureTypes::Left : GestureTypes::Right, 3);
        const bool closes = interaction->isValidInteraction(GestureTypes::Swipe, rtl ? GestureTypes::Right : GestureTypes::Left, 3);
        if (!opens && !closes) return;
        connect(interaction.data(), &GestureInteraction::interactionEnded, this, [this, interaction, opens] {
            if (interaction->extrapolatePercentage(100) <= 0.3) return;
            if (opens) show();
            else close();
        });
    });
}

Gateway::~Gateway() {
}

Gateway* Gateway::instance() {
    if (!d->instance) d->instance = new Gateway();
    return d->instance;
}

void Gateway::place() {
    // The launcher: a column in the middle, from under the bar's resting
    // height to near the bottom (it uses what it needs of that)
    const QRect screen = primaryScreenGeometry();
    const int width = Launcher::columnWidth(screen.width());
    const int top = qMax(56, int(screen.height() * 0.094));
    const QRect column(screen.left() + (screen.width() - width) / 2, screen.top() + top, width, screen.height() - top - 16);
    if (Compositor::isActive()) {
        this->setGeometry(screen);
        d->launcher->setGeometry(column.translated(-screen.topLeft()));
    } else {
        this->setGeometry(column);
        d->launcher->setGeometry(QRect(QPoint(0, 0), column.size()));
    }
    applyShape();
}

void Gateway::applyShape() {
    // With a compositor the window is see-through where nothing is drawn.
    // Without one, cut it to the bar and the panel
    if (Compositor::isActive()) {
        this->clearMask();
        return;
    }
    const QRegion shape = d->launcher->shape().translated(d->launcher->pos());
    if (!shape.isEmpty()) this->setMask(shape);
}

void Gateway::show() {
    d->open = true;
    d->leave->stop();
    this->setWindowOpacity(1);
    place();
    d->launcher->opened();
    d->scrim->animateTo(1.0);

    QDialog::show();
    this->raise(); // over the bar, which may have just expanded
    QDialog::activateWindow();
    d->launcher->setFocus();
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeMenu);
    StateManager::gatewayManager()->setOpen(true);
}

bool Gateway::isOpen() const {
    return this->isVisible() && d->open;
}

void Gateway::close() {
    if (!d->open) return;
    d->open = false;
    StateManager::gatewayManager()->setOpen(false);
    d->scrim->animateTo(0.0);
    // Fading a window takes a compositor; without one it just goes
    if (Compositor::isActive() && M3Motion::expressive()) {
        d->leave->start();
    } else {
        QDialog::hide();
    }
}

void Gateway::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    if (!Compositor::isActive()) return;
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    QColor scrim = M3::currentScheme().scrim;
    scrim.setAlphaF(float(scrimOpacity * qBound(0.0, d->scrim->currentValue().toDouble(), 1.0)));
    p.fillRect(this->rect(), scrim);
}

void Gateway::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && !this->isActiveWindow()) this->close();
}

void Gateway::mousePressEvent(QMouseEvent* event) {
    // On the scrim: outside the launcher
    Q_UNUSED(event)
    this->close();
}
