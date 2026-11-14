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
#include "notificationsdrawer.h"

#include <QPainterPath>
#include "ui_notificationsdrawer.h"

#include "notificationsdrawerwidget.h"
#include "notificationtracker.h"
#include <QElapsedTimer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <desksound.h>
#include <compositor.h>
#include <gatewaymanager.h>
#include <libcontemporary_global.h>
#include <quietmodemanager.h>
#include <statemanager.h>

struct NotificationsDrawerPrivate {
        NotificationTracker* tracker;
        QList<NotificationsDrawerWidget*> widgets;

        QScreen* oldPrimaryScreen = nullptr;

        // The pop-ups (newest first), stepped from one frame timer
        QTimer frames;
        QElapsedTimer clock;
        qint64 lastFrame = 0;

        // M3 snackbars sit at the bottom centre, in a window of their own
        QDialog* snackbarWindow = nullptr;
};

namespace {
    constexpr int snackbarMaxWidth = 672; // M3's widest snackbar
    constexpr int snackbarMargin = 24; // from the bottom of the screen
} // namespace

NotificationsDrawer::NotificationsDrawer(NotificationTracker* tracker) :
    QDialog(nullptr),
    ui(new Ui::NotificationsDrawer) {
    ui->setupUi(this);

    d = new NotificationsDrawerPrivate();
    d->tracker = tracker;
    connect(tracker, &NotificationTracker::destroyed, this, &NotificationsDrawer::deleteLater);
    connect(tracker, &NotificationTracker::newNotification, this, &NotificationsDrawer::showNotification);

    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setWindowFlag(Qt::WindowStaysOnTopHint);
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeNotification);

    connect(StateManager::barManager(), &BarManager::barHeightChanged, this, &NotificationsDrawer::updateGeometry);

    // bspwm doesn't stack notification windows, and raises the window it
    // focuses above everything: pop-ups that were already up ended up under
    // the focused window. Come back on top whenever focus moves
    connect(DesktopWm::instance(), &DesktopWm::activeWindowChanged, this, [this] {
        if (this->isVisible()) this->raise();
    });
    connect(StateManager::gatewayManager(), &GatewayManager::gatewayWidthChanged, this, &NotificationsDrawer::updateGeometry);
    d->snackbarWindow = new QDialog(nullptr);
    d->snackbarWindow->setAttribute(Qt::WA_TranslucentBackground);
    d->snackbarWindow->setAttribute(Qt::WA_ShowWithoutActivating);
    d->snackbarWindow->setWindowFlag(Qt::FramelessWindowHint);
    d->snackbarWindow->setWindowFlag(Qt::WindowStaysOnTopHint);
    auto* snackbarLayout = new QVBoxLayout(d->snackbarWindow);
    snackbarLayout->setContentsMargins(0, 0, 0, 0);
    snackbarLayout->addWidget(ui->hudWidget);
    ui->hudWidget->setRisesFromBelow(true);
    DesktopWm::setSystemWindow(d->snackbarWindow, DesktopWm::SystemWindowTypeNotification);
    connect(ui->hudWidget, &HudWidget::shouldShowChanged, this, &NotificationsDrawer::updateSnackbarGeometry);

    connect(qApp, &QApplication::primaryScreenChanged, this, &NotificationsDrawer::updateGeometry);
    d->frames.setInterval(16);
    d->clock.start();
    connect(&d->frames, &QTimer::timeout, this, &NotificationsDrawer::frame);
    updateGeometry();
}

NotificationsDrawer::~NotificationsDrawer() {
    delete d->snackbarWindow;
    delete ui;
}

void NotificationsDrawer::updateSnackbarGeometry() {
    QWidget* window = d->snackbarWindow;
    if (!ui->hudWidget->shouldShow() || ui->hudWidget->height() == 0) {
        window->hide();
        return;
    }
    // Grows upwards from a fixed bottom edge, so the card rises into place
    const QRect screen = qApp->primaryScreen()->geometry();
    // As wide as its words, within M3's 344-672 dp at most
    const QSize size(qMin(ui->hudWidget->preferredWidth(), qMin(snackbarMaxWidth, screen.width() - 2 * snackbarMargin)), ui->hudWidget->height());
    window->setFixedSize(size);
    window->move(screen.center().x() - size.width() / 2, screen.bottom() - snackbarMargin - size.height());

    // Cut the window down to the pill: it's what shows without a compositor,
    // and what takes clicks with one (there a pixel wider, so the pill's
    // smooth edge isn't cut by the mask's stepped one)
    QPainterPath card;
    const int spare = Compositor::isActive() ? 1 : 0;
    const QRectF pill = QRectF(ui->hudWidget->cardRect().translated(ui->hudWidget->mapTo(window, QPoint(0, 0)))).adjusted(-spare, -spare, spare, spare);
    card.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    QRegion region(card.toFillPolygon().toPolygon());
    if (!region.isEmpty()) window->setMask(region);
    window->show();
    window->raise();
}

void NotificationsDrawer::setFixedHeight(int height) {
    QDialog::setFixedHeight(height);

    // Work around a Qt bug that causes the native window not to be updated on Wayland for some reason
    this->windowHandle()->setGeometry(this->geometry());
}

void NotificationsDrawer::setFixedWidth(int width) {
    QWidget::setFixedWidth(width);

    // Work around a Qt bug that causes the native window not to be updated on Wayland for some reason
    this->windowHandle()->setGeometry(this->geometry());
}

void NotificationsDrawer::updateGeometry() {
    QScreen* primaryScreen = qApp->primaryScreen();
    if (d->oldPrimaryScreen != primaryScreen && d->oldPrimaryScreen) {
        disconnect(d->oldPrimaryScreen, &QScreen::geometryChanged, this, &NotificationsDrawer::updateGeometry);
    }

    if (!d->oldPrimaryScreen) {
        connect(primaryScreen, &QScreen::geometryChanged, this, &NotificationsDrawer::updateGeometry);
    }
    d->oldPrimaryScreen = primaryScreen;

    // The pebbles' rail runs down the right edge, under the bar; the window
    // is cut down to them (no compositor: the "translucent" rest is black)
    const QRect screen = primaryScreen->geometry();
    const int top = StateManager::barManager()->barHeight() + 16;
    this->setFixedWidth(NotificationsDrawerWidget::CardWidth + 2 * NotificationsDrawerWidget::Edge);
    this->setFixedHeight(qMax(1, screen.height() - top - 16));
    if (this->layoutDirection() == Qt::RightToLeft) {
        this->move(screen.left() + 8, screen.top() + top);
    } else {
        this->move(screen.right() + 1 - 8 - this->width(), screen.top() + top);
    }
    frame();
}

void NotificationsDrawer::frame() {
    const qint64 now = d->clock.elapsed();
    const double dt = qBound(0.001, (now - d->lastFrame) / 1000.0, 0.05);
    d->lastFrame = now;

    // The newest one is a card; the ones before fold into the rail
    bool moving = false, front = true;
    double y = 0;
    QRegion shown;
    for (NotificationsDrawerWidget* w : std::as_const(d->widgets)) {
        w->setFront(front && !w->leaving());
        if (!w->leaving()) front = false;
        moving |= w->step(dt);
        w->setGeometry(0, qRound(y), this->width(), w->plateHeight());
        y += w->slot();
        const QPainterPath outline = w->outline();
        if (!outline.isEmpty()) shown += QRegion(outline.translated(w->pos()).toFillPolygon().toPolygon());
    }
    for (NotificationsDrawerWidget* w : QList(d->widgets)) {
        if (!w->done()) continue;
        d->widgets.removeOne(w);
        w->deleteLater();
    }

    if (shown.isEmpty()) {
        this->hide();
    } else {
        this->setMask(shown);
        if (!this->isVisible()) {
            this->show();
            this->raise();
        }
    }
    if (!moving || d->widgets.isEmpty()) d->frames.stop();
}

void NotificationsDrawer::showNotification(NotificationPtr notification) {
    switch (StateManager::quietModeManager()->currentMode()) {
        case QuietModeManagerTd::CriticalOnly:
            if (notification->urgency() != Notification::Critical) return;
            break;
        case QuietModeManagerTd::NoNotifications:
            return;
        case QuietModeManagerTd::Mute:
            // Mute silences sound, it doesn't hide notifications (it follows
            // the speaker's mute: a muted speaker hid every pop-up, a
            // pairing code included)
            break;
        default:
            break;
    }

    // Its sound: the app's own file or theme sound, else theDesk's. Low
    // urgency is quiet, and an app that plays its own asks for none
    if (notification->urgency() != Notification::Low && !notification->soundSuppressed()) {
        const DeskSound::Kind kind = notification->urgency() == Notification::Critical ? DeskSound::Critical : DeskSound::Notification;
        DeskSound* sound = nullptr;
        if (!notification->soundFile().isEmpty()) sound = DeskSound::playFile(notification->soundFile(), kind);
        if (!sound && !notification->soundName().isEmpty()) sound = DeskSound::play(notification->soundName(), kind);
        // Critical ones have a sound of their own (falls back to the usual)
        if (!sound) DeskSound::play(kind == DeskSound::Critical ? QStringLiteral("message-new-instant-critical") : QStringLiteral("message-new-instant"), kind);
    }

    NotificationsDrawerWidget* w = new NotificationsDrawerWidget(notification, d->tracker, this);
    QTimer::singleShot(0, this, [this] {
        this->raise(); // a new one comes up on top, even if the window was already up
    });
    d->widgets.prepend(w);
    connect(w, &NotificationsDrawerWidget::animate, this, [this] {
        if (!d->frames.isActive()) {
            d->lastFrame = d->clock.elapsed() - 16;
            d->frames.start();
        }
    });
    w->show();
    emit w->animate();
}
