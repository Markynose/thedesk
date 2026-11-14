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
#include "systemsettingsleftpane.h"

#include <m3motion.h>
#include "ui_systemsettingsleftpane.h"

#include "../statuscenter/leftpanedelegate.h"
#include <libcontemporary_global.h>
#include <powermanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tvariantanimation.h>
#include <QHBoxLayout>
#include <QShortcut>
#include <QToolButton>

struct SystemSettingsLeftPanePrivate {
        bool isShowingLogOutRequired = false;
        QToolButton* search;
};

SystemSettingsLeftPane::SystemSettingsLeftPane(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SystemSettingsLeftPane) {
    ui->setupUi(this);

    d = new SystemSettingsLeftPanePrivate();

    ui->titleLabel->setBackButtonShown(true);
    ui->mainList->setIconSize(SC_DPI_T(QSize(32, 32), QSize));
    ui->mainList->setItemDelegate(new LeftPaneDelegate(this));

    // Search over the list (Android's Settings home): a round button on the rail, a bar when the list is open
    d->search = new QToolButton(this);
    d->search->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    d->search->setToolTip(tr("Search settings"));
    d->search->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
    d->search->setProperty("m3ButtonSize", QStringLiteral("m"));
    auto* searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(20, 4, 20, 8);
    searchRow->addWidget(d->search);
    searchRow->addStretch();
    ui->verticalLayout->insertLayout(ui->verticalLayout->indexOf(ui->mainList), searchRow);
    connect(d->search, &QToolButton::clicked, this, [this] {
        emit searchRequested(d->search);
    });
    auto* find = new QShortcut(QKeySequence::Find, this);
    connect(find, &QShortcut::activated, this, [this] {
        emit searchRequested(d->search);
    });

    ui->logoutRequiredWidget->setFixedHeight(0);
    ui->logoutRequiredWidget->setVisible(false);
    ui->logoutButton->setProperty("type", "destructive");

    connect(StateManager::statusCenterManager(), &StatusCenterManager::requestLogout, this, [=] {
        if (d->isShowingLogOutRequired) return;
        d->isShowingLogOutRequired = true;

        tVariantAnimation* anim = new tVariantAnimation(this);
        anim->setStartValue(0);
        anim->setEndValue(ui->logoutRequiredWidget->sizeHint().height());
        anim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
        anim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
        connect(anim, &tVariantAnimation::valueChanged, this, [=](QVariant value) {
            ui->logoutRequiredWidget->setFixedHeight(value.toInt());
        });
        connect(anim, &tVariantAnimation::finished, anim, &tVariantAnimation::deleteLater);
        anim->start();

        ui->logoutRequiredWidget->setVisible(true);
    });
}

SystemSettingsLeftPane::~SystemSettingsLeftPane() {
    delete d;
    delete ui;
}

void SystemSettingsLeftPane::appendItem(QListWidgetItem* item) {
    ui->mainList->addItem(item);
}

void SystemSettingsLeftPane::insertItem(int index, QListWidgetItem* item) {
    ui->mainList->insertItem(index, item);
}

void SystemSettingsLeftPane::removeItem(QListWidgetItem* item) {
    ui->mainList->removeItemWidget(item);
}

void SystemSettingsLeftPane::on_titleLabel_backButtonClicked() {
    StateManager::statusCenterManager()->returnToRootMenu();
}

void SystemSettingsLeftPane::on_mainList_currentRowChanged(int currentRow) {
    emit indexChanged(currentRow);
}

void SystemSettingsLeftPane::on_mainList_clicked(const QModelIndex& index) {
    emit enterMenu(index.row());
}

void SystemSettingsLeftPane::on_logoutButton_clicked() {
    StateManager::instance()->powerManager()->showPowerOffConfirmation(PowerManager::LogOut);
}

void SystemSettingsLeftPane::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const bool open = this->width() > 200; // the list, not the rail
    d->search->setToolButtonStyle(open ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    d->search->setText(open ? tr("Search settings") : QString());
}

void SystemSettingsLeftPane::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}
