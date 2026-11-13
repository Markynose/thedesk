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
#include "endsession.h"
#include "ui_endsession.h"

#include "transparentdialog.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <Wm/desktopwm.h>
#include <onboardingmanager.h>
#include <powermanager.h>
#include <statemanager.h>
#include <tpopover.h>
#include <QGridLayout>
#include <m3usercard.h>
#include <tvariantanimation.h>

struct EndSessionPrivate {
        EndSessionButton* timedButton = nullptr;
};

EndSession::EndSession(PowerManager::PowerOperation operation, QString message, QStringList flags, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::EndSession) {
    ui->setupUi(this);

    d = new EndSessionPrivate();


    ui->rebootInstallUpdatesButton->setVisible(false);
    if (operation != PowerManager::All) {
        ui->powerOffButton->setVisible(false);
        ui->rebootButton->setVisible(false);
        ui->logoutButton->setVisible(false);
        ui->suspendButton->setVisible(false);
        ui->lockButton->setVisible(false);
        ui->screenOffButton->setVisible(false);
        ui->switchUsersButton->setVisible(false);
        ui->hibernateButton->setVisible(false);
    }

    switch (operation) {
        case PowerManager::All:
            if (StateManager::onboardingManager() && StateManager::onboardingManager()->isOnboardingRunning()) {
                ui->lockButton->setVisible(false);
                ui->screenOffButton->setVisible(false);
            }

            Q_FALLTHROUGH();
        case PowerManager::PowerOff:
            d->timedButton = ui->powerOffButton;
            break;
        case PowerManager::Reboot:
            if (flags.contains("update")) {
                d->timedButton = ui->rebootInstallUpdatesButton;
            } else {
                d->timedButton = ui->rebootButton;
            }
            break;
        case PowerManager::LogOut:
            d->timedButton = ui->logoutButton;
            break;
        case PowerManager::Suspend:
            d->timedButton = ui->suspendButton;
            break;
        case PowerManager::Lock:
            d->timedButton = ui->lockButton;
            break;
        case PowerManager::TurnOffScreen:
            d->timedButton = ui->screenOffButton;
            break;
        case PowerManager::Hibernate:
            d->timedButton = ui->hibernateButton;
            break;
    }

    d->timedButton->setVisible(true);
    d->timedButton->setProperty("type", "destructive");

    // No countdown: nothing happens until you pick something (user's choice)
    if (message.isEmpty()) {
        // The user card: picture in its shape, "Ready to head out," and the
        // name springing to its heaviest, widest Roboto Flex
        auto* card = new M3UserCard(this);
        card->setCardSize(M3UserCard::Compact);
        card->setGreeting(tr("Ready to head out,"));
        card->setNameSuffix(QStringLiteral("?"));
        ui->verticalLayout_4->insertWidget(0, card);
        ui->descriptionLabel->setVisible(false);
    } else {
        QString text = message;
        text.replace(QStringLiteral(" in %n seconds if you don't do anything"), QString());
        ui->descriptionLabel->setText(text.contains(QLatin1String("%1")) ? text.arg(DesktopWm::userDisplayName()) : text);
        ui->descriptionLabel->setWordWrap(true);
        QFont heading = ui->descriptionLabel->font();
        heading.setFamily(QStringLiteral("Roboto Flex"));
        heading.setPixelSize(24);
        heading.setVariableAxis(QFont::Tag("wght"), 700);
        heading.setVariableAxis(QFont::Tag("wdth"), 112);
        ui->descriptionLabel->setFont(heading);
    }
    d->timedButton->setDefault(true);
    d->timedButton->setFocus();

    // Tiles: power off and restart as hero tiles side by side, the rest in a
    // grid of three, in Android 16's power menu manner
    QList<EndSessionButton*> heroes, tiles;
    for (EndSessionButton* b : {ui->powerOffButton, ui->rebootButton, ui->rebootInstallUpdatesButton}) {
        b->setRole(b == ui->powerOffButton ? EndSessionButton::Role::Primary : EndSessionButton::Role::Secondary);
        if (!b->isHidden()) heroes.append(b);
    }
    for (EndSessionButton* b : {ui->suspendButton, ui->lockButton, ui->logoutButton, ui->switchUsersButton, ui->screenOffButton, ui->hibernateButton}) {
        b->setRole(EndSessionButton::Role::Tile);
        if (!b->isHidden()) tiles.append(b);
    }
    if (heroes.isEmpty() && !tiles.isEmpty() && tiles.size() == 1) {
        // A single operation asked for (log out, suspend...): make it the hero
        tiles.first()->setRole(EndSessionButton::Role::Primary);
        heroes.append(tiles.takeFirst());
    }
    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    grid->setContentsMargins(0, 8, 0, 0);
    for (int i = 0; i < heroes.size(); i++) {
        for (QLayout* l : {static_cast<QLayout*>(ui->verticalLayout_2), static_cast<QLayout*>(ui->verticalLayout_3)}) l->removeWidget(heroes[i]);
        const int span = 6 / int(heroes.size()); // a six-column grid
        grid->addWidget(heroes[i], 0, i * span, 1, span);
    }
    for (int i = 0; i < tiles.size(); i++) {
        for (QLayout* l : {static_cast<QLayout*>(ui->verticalLayout_2), static_cast<QLayout*>(ui->verticalLayout_3)}) l->removeWidget(tiles[i]);
        grid->addWidget(tiles[i], 1 + i / 3, (i % 3) * 2, 1, 2);
    }
    for (int c = 0; c < 6; c++) grid->setColumnStretch(c, 1);
    // The old list's spacers would leave a gap above the tiles
    for (int i = ui->verticalLayout_2->count() - 1; i >= 0; i--) {
        if (ui->verticalLayout_2->itemAt(i)->spacerItem()) delete ui->verticalLayout_2->takeAt(i);
    }
    ui->verticalLayout_2->addLayout(grid);

    ui->centeredWidget->setFixedWidth(SC_DPI(600));
    ui->centeredWidgetUpdates->setFixedWidth(SC_DPI(600));
    ui->powerOffButton->setProperty("type", "destructive");
    ui->rebootButton->setProperty("type", "destructive");
    ui->titleLabel->setBackButtonShown(true);
    ui->updatesAvailableTitle->setBackButtonShown(true);
    ui->stackedWidget->setCurrentAnimation(tStackedWidget::SlideHorizontal);
}

int EndSession::preferredHeight() const {
    // tStackedWidget reports the screen's height; the menu page knows its own
    return ui->mainPage->sizeHint().height();
}

EndSession::~EndSession() {
    delete d;
    delete ui;
}

tPromise<void>* EndSession::showDialog(PowerManager::PowerOperation operation, QString message, QStringList flags) {
    return tPromise<void>::runOnSameThread([=](tPromiseFunctions<void>::SuccessFunction res, tPromiseFunctions<void>::FailureFunction rej) {
        Q_UNUSED(rej)

        TransparentDialog* dialog = new TransparentDialog();
        dialog->showFullScreen();

        QTimer::singleShot(500, [=] {
            EndSession* popoverContents = new EndSession(operation, message, flags);

            tPopover* popover = new tPopover(popoverContents);
            popover->setPopoverSide(tPopover::Bottom);
            // A bottom sheet's "width" is its height. heightForWidth() is -1
            // for this layout, which left the sheet cut off mid-screen: use
            // the content's own height instead
            int height = popoverContents->preferredHeight();
            popover->setPopoverWidth(qMin(height, dialog->height() - 48));
            connect(popoverContents, &EndSession::done, popover, &tPopover::dismiss);
            connect(popover, &tPopover::dismissed, popoverContents, &EndSession::deleteLater);
            connect(popover, &tPopover::dismissed, [=] {
                popover->deleteLater();
                dialog->deleteLater();
                popoverContents->deleteLater();

                res();
            });
            popover->show(dialog);
        });
    });
}

void EndSession::on_titleLabel_backButtonClicked() {
    emit done();
}

void EndSession::on_powerOffButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::PowerOff);
    emit done();
}

void EndSession::on_rebootButton_clicked() {
    if (QFile("/var/lib/PackageKit/prepared-update").exists() && !QFile::exists("/system-update")) {
        // Ask the user to install updates
        ui->stackedWidget->setCurrentWidget(ui->updatesAvailablePage);
    } else {
        StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Reboot);
        emit done();
    }
}

void EndSession::on_logoutButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::LogOut);
    emit done();
}

void EndSession::on_suspendButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Suspend);
    emit done();
}

void EndSession::on_lockButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Lock);
    emit done();
}

void EndSession::on_screenOffButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::TurnOffScreen);
    emit done();
}

void EndSession::on_switchUsersButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::SwitchUsers);
    emit done();
}

void EndSession::on_hibernateButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Hibernate);
    emit done();
}

void EndSession::on_updatesAvailableTitle_backButtonClicked() {
    ui->stackedWidget->setCurrentWidget(ui->mainPage);
}

void EndSession::on_rebootNoUpdateButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Reboot);
    emit done();
}

void EndSession::on_rebootUpdateButton_clicked() {
    // Reboot
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Reboot, {"update"});
    emit done();
}

void EndSession::on_stackedWidget_switchingFrame(int frame) {
    Q_UNUSED(frame) // there used to be a countdown to pause here
}

void EndSession::on_rebootInstallUpdatesButton_clicked() {
    StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Reboot, {"update"});
    emit done();
}

void EndSession::on_rebootButton_customContextMenuRequested(const QPoint& pos) {
    QMenu* menu = new QMenu();
    menu->addSection(tr("Advanced Reboot"));
    menu->addAction(QIcon::fromTheme("system-reboot"), tr("Reboot"), [=] {
        // Just reboot
        ui->rebootButton->click();
    });

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "CanRebootToFirmwareSetup");
    QDBusMessage msg = QDBusConnection::systemBus().call(message);

    if (msg.arguments().first().toString() == "yes") {
        menu->addAction(QIcon::fromTheme("system-reboot"), tr("Reboot into System UEFI Setup"), [=] {
            StateManager::instance()->powerManager()->performPowerOperation(PowerManager::Reboot, {"setup"});
            emit done();
        });
    }
    menu->popup(ui->rebootButton->mapToGlobal(pos));
}
