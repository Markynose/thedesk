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
#include "overlaywindow.h"
#include "ui_overlaywindow.h"

#include "Wm/desktopwm.h"
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <tscrim.h>
#include <tx11info.h>
// Xlib last: its macros clash with Qt headers
#include <X11/Xlib.h>

OverlayWindow::OverlayWindow(QWidget* parent) :
    QWidget(parent), ui(new Ui::OverlayWindow) {
    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlag(Qt::WindowStaysOnTopHint);
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        // Window managers like bspwm ignore raise requests from the windows
        // they manage, so a prompt could open under a window already on top
        // (System Settings): skip the window manager, as a lock screen would
        this->setWindowFlag(Qt::X11BypassWindowManagerHint);
    }

    tScrim::scrimForWidget(this)->setBlurEnabled(false);
}

OverlayWindow::~OverlayWindow() {
    delete ui;
}

void OverlayWindow::showEvent(QShowEvent* event) {
    DesktopWm::instance()->setSystemWindow(this, DesktopWm::SystemWindowTypeLockScreen);
    this->activateWindow(); // unmanaged windows take the keyboard themselves
}

namespace {
    // A compositing manager owns _NET_WM_CM_S0: then this window really is
    // translucent and needs no picture of the screen behind it
    bool compositing() {
        if (!tX11Info::isPlatformX11()) return true;
        Display* display = tX11Info::display();
        return XGetSelectionOwner(display, XInternAtom(display, "_NET_WM_CM_S0", False)) != 0;
    }
} // namespace

void OverlayWindow::setVisible(bool visible) {
    // Without a compositor a translucent window can't show what's under it
    // (the scrim turned the whole screen dark): keep a picture of the screen
    // from just before it opens and draw the scrim over that instead
    if (visible && !this->isVisible() && QGuiApplication::platformName() == QStringLiteral("xcb") && !compositing()) {
        QScreen* screen = QGuiApplication::screenAt(this->geometry().center());
        if (!screen) screen = QGuiApplication::primaryScreen();
        backdrop = screen->grabWindow(0, this->geometry().x() - screen->geometry().x(), this->geometry().y() - screen->geometry().y(), this->geometry().width(), this->geometry().height());
    } else if (!visible) {
        backdrop = QPixmap();
    }
    QWidget::setVisible(visible);
}

void OverlayWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    if (backdrop.isNull()) return;
    QPainter painter(this);
    painter.drawPixmap(this->rect(), backdrop);
}
