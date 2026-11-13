/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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
#include "fileassociationselect.h"
#include "ui_fileassociationselect.h"

#include <QFileInfo>
#include <QUrl>
#include <tpopover.h>
#include "setdefaultpopover.h"

struct FileAssociationSelectPrivate {
    QString mimeType;
    QList<ApplicationPointer> availableApplications;

    QString selectedApp;
    bool shouldSetDefault;

    QString lastUseApp;

    QSettings settings;
};

FileAssociationSelect::FileAssociationSelect(QUrl file, QString mimeType, QList<ApplicationPointer> availableApplications, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FileAssociationSelect) {
    ui->setupUi(this);
    d = new FileAssociationSelectPrivate();
    d->mimeType = mimeType;
    d->availableApplications = availableApplications;

    QSignalBlocker blocker(ui->stackedWidget);
    d->settings.beginGroup("assocselect");

    this->setWindowFlag(Qt::WindowStaysOnTopHint);

    QString filename;
    if (file.isLocalFile()) {
        QFileInfo fileInfo(file.toLocalFile());
        filename = fileInfo.fileName();
    } else {
        filename = file.toString();
    }

    ui->allAppsTitle->setBackButtonShown(true);
    ui->noApplicationLabel->setText(tr("Looks like there isn't an application installed that can open %1.\n\nYou can grab a new app to open this file, or you can try opening it with another app on this device.").arg(QLocale().quoteString(filename)));
    ui->multiAppsDescription->setText(tr("Select the app you'd like to use to open %1.").arg(QLocale().quoteString(filename)));
    ui->dismissButton->setProperty("type", "destructive");
    ui->dismissButtonMulti->setProperty("type", "destructive");

    ui->lastUseWidget->setVisible(false);
    ui->lastUseButtonNoAssoc->setVisible(false);
    if (d->settings.contains(mimeType) && d->mimeType != "application/octet-stream") {
        //Ask if they want to use this app again
        Application app(d->settings.value(mimeType).toString());
        if (app.isValid()) {
            QString appName = app.getProperty("Name").toString();
            ui->lastUseTitle->setText(tr("Use %1?").arg(QLocale().quoteString(appName)).toUpper());
            ui->lastUseMessage->setText(tr("You used %1 to open these files last time. Do you want to use it again?").arg(QLocale().quoteString(appName)));
            ui->lastUseButton->setText(tr("Open with %1").arg(QLocale().quoteString(appName)));
            ui->lastUseButton->setIcon(QIcon::fromTheme(app.getProperty("Icon").toString()));
            ui->lastUseButtonNoAssoc->setText(tr("Use %1 again").arg(QLocale().quoteString(appName)));
            ui->lastUseButtonNoAssoc->setIcon(QIcon::fromTheme(app.getProperty("Icon").toString()));
            ui->noApplicationLabel->setText(tr("Looks like there isn't an application installed that can open %1. Last time we asked, you used %2.\n\nYou can use %2 again, grab a new app to open this file, or you can try opening it with another app on this device.").arg(QLocale().quoteString(filename), QLocale().quoteString(appName)));

            ui->lastUseButtonNoAssoc->setVisible(true);
            ui->lastUseWidget->setVisible(true);
            d->lastUseApp = app.desktopEntry();
        }
    }

    ui->multiAssocPage->setFixedSize(SC_DPI_T(QSize(400, 600), QSize));
    ui->allAppsPage->setFixedSize(SC_DPI_T(QSize(400, 600), QSize));

    ui->stackedWidget->setCurrentAnimation(tStackedWidget::SlideHorizontal);
    if (availableApplications.isEmpty()) {
        ui->stackedWidget->setCurrentWidget(ui->noAssocPage, false);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->multiAssocPage, false);

        std::sort(d->availableApplications.begin(), d->availableApplications.end(), [ = ](const ApplicationPointer & first, const ApplicationPointer & second) {
            return first->getProperty("Name").toString().localeAwareCompare(second->getProperty("Name").toString()) < 0;
        });

        for (const ApplicationPointer& app : qAsConst(d->availableApplications)) {
            QListWidgetItem* item = new QListWidgetItem();
            item->setText(app->getProperty("Name").toString());
            item->setIcon(QIcon::fromTheme(app->getProperty("Icon").toString()));
            item->setData(Qt::UserRole, app->desktopEntry());
            ui->multiAppsList->addItem(item);
        }
    }

    for (const QString& application : Application::allApplications()) {
        ApplicationPointer app(new Application(application));

        //Make sure this app is good to be shown
        if (app->getProperty("Type", "").toString() != "Application") continue;
        if (app->getProperty("NoDisplay", false).toBool()) continue;
        if (!app->getStringList("OnlyShowIn", {"thedesk"}).contains("thedesk")) continue;
        if (app->getStringList("NotShowIn").contains("thedesk")) continue;

        QListWidgetItem* item = new QListWidgetItem();
        item->setText(app->getProperty("Name").toString());
        item->setIcon(QIcon::fromTheme(app->getProperty("Icon").toString()));
        item->setData(Qt::UserRole, app->desktopEntry());
        ui->allAppsList->addItem(item);
    }


    switch (ui->stackedWidget->currentIndex()) {
        case 0:
            this->setFixedSize(ui->stackedWidget->currentWidget()->sizeHint());
            break;
        case 1:
        case 2:
            this->setFixedSize(ui->stackedWidget->currentWidget()->size());
            break;
    }
}

FileAssociationSelect::~FileAssociationSelect() {
    delete ui;
    delete d;
}

ApplicationPointer FileAssociationSelect::selectedApplication() {
    return ApplicationPointer(new Application(d->selectedApp));
}

bool FileAssociationSelect::shouldSetDefault() {
    return d->shouldSetDefault;
}

void FileAssociationSelect::on_dismissButton_clicked() {
    this->reject();
}

void FileAssociationSelect::on_stackedWidget_switchingFrame(int frame) {
    tVariantAnimation* anim = new tVariantAnimation();
    anim->setStartValue(this->geometry());

    QRect newGeometry;
    switch (frame) {
        case 0:
            newGeometry.setSize(ui->stackedWidget->widget(frame)->sizeHint());
            break;
        case 1:
        case 2:
            newGeometry.setSize(ui->stackedWidget->widget(frame)->size());
            break;
    }

    newGeometry.moveCenter(this->geometry().center());

    anim->setEndValue(newGeometry);
    anim->setDuration(250);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        this->setFixedSize(value.toRect().size());
        this->setGeometry(value.toRect());
    });
    connect(anim, &tVariantAnimation::finished, anim, &tVariantAnimation::deleteLater);
    anim->start();
}

void FileAssociationSelect::on_allAppsTitle_backButtonClicked() {
    if (d->availableApplications.isEmpty()) {
        ui->stackedWidget->setCurrentWidget(ui->noAssocPage);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->multiAssocPage);
    }
}

void FileAssociationSelect::on_chooseAppNoAssoc_clicked() {
    ui->stackedWidget->setCurrentWidget(ui->allAppsPage);
}

void FileAssociationSelect::on_showAllAppsMultiApp_clicked() {
    ui->stackedWidget->setCurrentWidget(ui->allAppsPage);
}

void FileAssociationSelect::useApp(QString desktopEntry) {
    if (d->mimeType == "application/octet-stream") {
        d->selectedApp = desktopEntry;
        d->shouldSetDefault = false;
        accept();
        return;
    }

    SetDefaultPopover* setDefault = new SetDefaultPopover(d->mimeType, ApplicationPointer(new Application(desktopEntry)), this);
    tPopover* popover = new tPopover(setDefault);
    popover->setPopoverWidth(setDefault->sizeHint().height());
    popover->setPopoverSide(tPopover::Bottom);
    connect(setDefault, &SetDefaultPopover::accepted, this, [ = ](bool shouldSetDefault) {
        d->selectedApp = desktopEntry;
        d->shouldSetDefault = shouldSetDefault;
        d->settings.setValue(d->mimeType, desktopEntry);
        connect(popover, &tPopover::dismissed, this, &FileAssociationSelect::accept);
        popover->dismiss();
    });
    connect(setDefault, &SetDefaultPopover::rejected, this, [ = ] {
        popover->dismiss();
    });
    connect(popover, &tPopover::dismissed, popover, &tPopover::deleteLater);
    connect(popover, &tPopover::dismissed, setDefault, &SetDefaultPopover::deleteLater);
    popover->show(this->window());
}

void FileAssociationSelect::on_multiAppsList_itemActivated(QListWidgetItem* item) {
    useApp(item->data(Qt::UserRole).toString());
}

void FileAssociationSelect::on_allAppsList_itemActivated(QListWidgetItem* item) {
    useApp(item->data(Qt::UserRole).toString());
}

void FileAssociationSelect::on_dismissButtonMulti_clicked() {
    reject();
}

void FileAssociationSelect::on_lastUseButton_clicked() {
    d->selectedApp = d->lastUseApp;
    d->shouldSetDefault = false;
    accept();
}

void FileAssociationSelect::on_lastUseButtonNoAssoc_clicked() {
    d->selectedApp = d->lastUseApp;
    d->shouldSetDefault = false;
    accept();
}
