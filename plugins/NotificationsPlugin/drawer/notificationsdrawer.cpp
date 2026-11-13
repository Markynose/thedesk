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
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <compositor.h>
#include <gatewaymanager.h>
#include <libcontemporary_global.h>
#include <quietmodemanager.h>
#include <statemanager.h>

struct NotificationsDrawerPrivate {
        NotificationTracker* tracker;
        QList<NotificationsDrawerWidget*> widgets;

        QScreen* oldPrimaryScreen = nullptr;

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

bool NotificationsDrawer::eventFilter(QObject* watched, QEvent* event) {
    // Cards slide in by moving inside their widget: follow them with the mask
    if ((event->type() == QEvent::Move || event->type() == QEvent::Resize) && watched->objectName() == QStringLiteral("mainFrame")) {
        this->updateGeometry();
        return false;
    }
    if (event->type() == QEvent::LayoutRequest) {
        this->updateGeometry();
    }
    return false;
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

    this->setFixedWidth(400);
    this->setFixedHeight(this->sizeHint().height());

    QRect geometry;
    geometry.setSize(this->size());
    if (this->layoutDirection() == Qt::RightToLeft) {
        geometry.moveTopRight(primaryScreen->geometry().topRight() + QPoint(-StateManager::gatewayManager()->gatewayWidth(), StateManager::barManager()->barHeight() + 16));
    } else {
        geometry.moveTopLeft(primaryScreen->geometry().topLeft() + QPoint(StateManager::gatewayManager()->gatewayWidth() + 8, StateManager::barManager()->barHeight() + 16));
    }

    if (geometry.bottom() > primaryScreen->geometry().bottom()) geometry.moveBottom(primaryScreen->geometry().bottom());

    this->move(geometry.topLeft());

    // No compositor, so the "translucent" background would be black: cut
    // the window down to its cards (the snackbar and each notification)
    QRegion cards;
    auto addCard = [&cards](QRect rect, double radius) {
        if (rect.isEmpty()) return;
        QPainterPath path;
        path.addRoundedRect(QRectF(rect), radius, radius);
        cards += QRegion(path.toFillPolygon().toPolygon());
    };
    for (QWidget* w : d->widgets) {
        // The card itself (mainFrame) sits inside the widget's margin
        QWidget* card = w->findChild<QWidget*>(QStringLiteral("mainFrame"));
        if (!card) card = w;
        if (w->isVisible()) addCard(QRect(card->mapTo(this, QPoint(0, 0)), card->size()), 12);
    }
    if (!cards.isEmpty()) this->setMask(cards);

    if (d->widgets.count() == 0) {
        this->hide();
    } else {
        this->show();
    }
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

    NotificationsDrawerWidget* w = new NotificationsDrawerWidget(notification, d->tracker, this);
    w->installEventFilter(this);
    if (QWidget* card = w->findChild<QWidget*>(QStringLiteral("mainFrame"))) card->installEventFilter(this);
    d->widgets.append(w);
    ui->notificationsLayout->addWidget(w);

    this->updateGeometry();

    connect(w, &NotificationsDrawerWidget::dismiss, this, [=] {
        d->widgets.removeOne(w);
        ui->notificationsLayout->removeWidget(w);
        w->deleteLater();

        this->updateGeometry();
    });

    w->show();
    this->updateGeometry();
}
