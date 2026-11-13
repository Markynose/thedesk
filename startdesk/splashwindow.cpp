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
#include "splashwindow.h"

#include <QTimer>

#include <m3motion.h>
#include "ui_splashwindow.h"

#include "splash/splashcontroller.h"
#include <QScreen>
#include <tapplication.h>
#include <tvariantanimation.h>

SplashWindow::SplashWindow(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SplashWindow) {
    ui->setupUi(this);

    // Unmanaged (override-redirect): bspwm decides stacking for windows it
    // manages and ignores our raises, while the bar (also unmanaged) puts
    // itself on top. Outside the window manager, our own raises win.
    this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::FramelessWindowHint);
    // A translucent (ARGB) window: with picom running, the exit's hole is
    // painted transparent instead of reshaping the window every frame
    // (LivingTypeView); without a compositor every pixel is painted opaque
    this->setAttribute(Qt::WA_TranslucentBackground);
    // ...which leaves the other pages with no backing: the crash report and
    // the questions paint their own (the window colour), opaque
    for (QWidget* page : {static_cast<QWidget*>(ui->crashPage), static_cast<QWidget*>(ui->questionPage)}) page->setAutoFillBackground(true);

    // The bar raises itself when it appears, which would put it on top of the
    // loading screen; stay on top until our exit reveals it through the hole
    auto* keepOnTop = new QTimer(this);
    keepOnTop->setInterval(50); // only while the splash is up
    connect(keepOnTop, &QTimer::timeout, this, [this] {
        if (this->isVisible()) this->raise();
    });
    connect(SplashController::instance(), &SplashController::starting, keepOnTop, qOverload<>(&QTimer::start));
    connect(SplashController::instance(), &SplashController::hideSplashes, this, [this] {
        this->raise(); // the bar is about to appear: be on top as the exit starts
    });
    keepOnTop->start();
    ui->stackedWidget->setCurrentAnimation(tStackedWidget::Fade);

    connect(SplashController::instance(), &SplashController::starting, this, [=] {
        // After a crash the loading screen says so (startdesk restarts theDesk)
        ui->splashPage->setRecovery(SplashController::instance()->isRecovering());
        ui->stackedWidget->setCurrentWidget(ui->splashPage, this->isVisible());
        if (!this->isVisible()) this->clearMask(); // the last exit's iris
        this->showFullScreen();
    });
    connect(SplashController::instance(), &SplashController::hideSplashes, this, [=] {
        this->hide();
    });
    connect(SplashController::instance(), &SplashController::crash, this, [=] {
        ui->stackedWidget->setCurrentWidget(ui->crashPage, this->isVisible());
        if (!this->isVisible()) this->clearMask(); // the last exit's iris
        this->showFullScreen();
    });
    connect(SplashController::instance(), &SplashController::startFail, this, [=] {
        ui->stackedWidget->setCurrentWidget(ui->crashPage, this->isVisible());
        if (!this->isVisible()) this->clearMask(); // the last exit's iris
        this->showFullScreen();
    });
    connect(SplashController::instance(), &SplashController::question, this, [=](QString title, QString message) {
        ui->questionTitle->setText(title);
        ui->questionText->setText(message);
        ui->stackedWidget->setCurrentWidget(ui->questionPage, this->isVisible());
        if (!this->isVisible()) this->clearMask(); // the last exit's iris
        this->showFullScreen();
    });

    this->setGeometry(tApplication::primaryScreen()->geometry());
}

SplashWindow::~SplashWindow() {
    delete ui;
}

void SplashWindow::hide() {
    // Play the splash's exit (a window can't fade without a compositor), then
    // hide; other pages just hide
    if (ui->stackedWidget->currentWidget() == ui->splashPage && this->isVisible()) {
        connect(ui->splashPage, &SplashWidget::exitFinished, this, [this] {
            if (auto* timer = this->findChild<QTimer*>()) timer->stop();
            // Keep the iris mask on: clearing it here showed the whole window
            // again for a few frames before it unmapped (the last picture from
            // before the exit: a blink). It's cleared before the next showing.
            QDialog::hide();
        }, Qt::SingleShotConnection);
        ui->splashPage->playExit();
    } else {
        QDialog::hide();
    }
}

void SplashWindow::on_yesButton_clicked() {
    SplashController::instance()->respond(true);
    ui->stackedWidget->setCurrentWidget(ui->splashPage);
}

void SplashWindow::on_noButton_clicked() {
    SplashController::instance()->respond(false);
    ui->stackedWidget->setCurrentWidget(ui->splashPage);
}
