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
#include "transparentdialog.h"
#include "ui_transparentdialog.h"

#include <Wm/desktopwm.h>
#include <QGuiApplication>
#include "compositor.h"
#include <QPainter>
#include <QScreen>
#include <tscrim.h>

TransparentDialog::TransparentDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::TransparentDialog) {
    ui->setupUi(this);

//    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setWindowFlag(Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        // bspwm ignores raise requests from the windows it manages: skip it,
        // so the sheet is always on top of whatever is open
        this->setWindowFlag(Qt::X11BypassWindowManagerHint);
    }

    tScrim::scrimForWidget(this)->setBlurEnabled(false);
}

TransparentDialog::~TransparentDialog() {
    delete ui;
}


void TransparentDialog::showEvent(QShowEvent *event)
{
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeLockScreen);
    this->activateWindow(); // unmanaged windows take the keyboard themselves
}

void TransparentDialog::setVisible(bool visible) {
    // Without a compositor a translucent window shows black, not what's under
    // it: draw the scrim over a picture of the screen from just before
    if (visible && !this->isVisible() && QGuiApplication::platformName() == QStringLiteral("xcb")) {
        QScreen* screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
        // With a compositor the window really is translucent: no picture needed
        backdrop = Compositor::isActive() ? QPixmap() : screen->grabWindow(0);
        // Unmanaged, so no window manager makes it full screen: cover the
        // screen ourselves (showFullScreen alone left a small floating window)
        this->setGeometry(screen->geometry());
    } else if (!visible) {
        backdrop = QPixmap();
    }
    QDialog::setVisible(visible);
}

void TransparentDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    if (backdrop.isNull()) return;
    QPainter painter(this);
    painter.drawPixmap(this->rect(), backdrop);
}
