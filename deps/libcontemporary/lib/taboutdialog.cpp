/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "taboutdialog.h"
#include "ui_taboutdialog.h"

#include "private/licensetextpopover.h"
#include "tapplication.h"
#include "tcontentsizer.h"
#include "tcsdtools.h"
#include "ticon.h"
#include "tpopover.h"
#include "tscrim.h"
#include <QDesktopServices>
#include <QPushButton>
#include <QScroller>
#include <QToolButton>

struct tAboutDialogPrivate {
        tCsdTools csd;
};

tAboutDialog::tAboutDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::tAboutDialog) {
    ui->setupUi(this);
    tIcon::processWidgetLater(this);
    this->setWindowFlag(Qt::WindowMinimizeButtonHint, false);
    this->setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    tScrim::setScrimProxy(this->window(), ui->centralWidget);

    d = new tAboutDialogPrivate();
    d->csd.installMoveAction(ui->topWidget);
    d->csd.installResizeAction(this);

    if (tCsdGlobal::windowControlsEdge() == tCsdGlobal::Left) {
        ui->leftCsdLayout->addWidget(d->csd.csdBoxForWidget(this));
    } else {
        ui->rightCsdLayout->addWidget(d->csd.csdBoxForWidget(this));
    }

    ui->topWidget->setFixedHeight(48);
#ifdef Q_OS_MAC
    ui->topWidget->setFixedHeight(24);
//    ui->topWidget->setContentsMargins(12, 0, 12, 0);
#endif
    ui->centralWidget->layout()->removeWidget(ui->topWidget);
    ui->topWidget->setFixedWidth(this->width());
    ui->topWidget->raise();
    ui->scrollAreaWidgetContents->setContentsMargins(0, ui->topWidget->height(), 0, 0);

    this->setMinimumWidth(700);

    this->setWindowTitle((tr("About %1").arg(tApplication::applicationName())));
    ui->applicationIcon->setPixmap(tApplication::applicationIcon().pixmap(QSize(ui->applicationTitle->sizeHint().height(), ui->applicationTitle->sizeHint().height())));
    ui->applicationTitle->setText(tApplication::applicationName());
    ui->genericName->setText(tApplication::genericName());
    ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Preferred, QSizePolicy::Expanding);
    new tContentSizer(ui->headWidget);
    new tContentSizer(ui->versionsWidget);
    new tContentSizer(ui->copyrightWidget);

    //    ui->versionsWidget->layout()->setContentsMargins(15 + ui->applicationTitle->sizeHint().height(), 9, 9, 9);
    //    ui->copyrightLayout->layout()->setContentsMargins(15 + ui->applicationTitle->sizeHint().height(), 9, 9, 9);

    if (tApplication::haveApplicationUrl(tApplication::Website)) {
        auto button = new QPushButton(this);
        button->setText(tr("Website"));
        button->setIcon(QIcon::fromTheme("text-html"));
        connect(button, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(tApplication::applicationUrl(tApplication::Website));
        });
        ui->buttonLayout->addWidget(button);
    }
    if (tApplication::haveApplicationUrl(tApplication::FileBug)) {
        auto button = new QPushButton(this);
        button->setText(tr("File Bug"));
        button->setIcon(QIcon::fromTheme("tools-report-bug"));
        connect(button, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(tApplication::applicationUrl(tApplication::FileBug));
        });
        ui->buttonLayout->addWidget(button);
    }
    if (tApplication::haveApplicationUrl(tApplication::Sources)) {
        auto button = new QPushButton(this);
        button->setText(tr("Sources"));
        button->setIcon(QIcon::fromTheme("commit"));
        connect(button, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(tApplication::applicationUrl(tApplication::Sources));
        });
        ui->buttonLayout->addWidget(button);
    }

    QList<QPair<QString, QString>> softwareVersions = tApplication::versions();
    for (int i = 0; i < softwareVersions.count(); i++) {
        auto* label1 = new QLabel(this);
        label1->setText(softwareVersions.at(i).first);
        label1->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        ui->versionsLayout->addWidget(label1, i + 1, 0);
        ui->versionsLayout->setAlignment(label1, Qt::AlignLeft);

        auto* label2 = new QLabel(this);
        label2->setText(softwareVersions.at(i).second);
        label2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        ui->versionsLayout->addWidget(label2, i + 1, 1);
        ui->versionsLayout->setAlignment(label2, Qt::AlignLeft);
    }

    QFont copyrightFont = this->font();
    if (copyrightFont.pixelSize() == -1) {
        copyrightFont.setPointSizeF(copyrightFont.pointSizeF() * 0.75);
    } else {
        copyrightFont.setPixelSize(copyrightFont.pixelSize() * 0.75);
    }
    QStringList copyrightLines = tApplication::copyrightLines();
    for (int i = 0; i < copyrightLines.count(); i++) {
        auto* label = new QLabel(this);
        label->setText(copyrightLines.at(i));
        label->setWordWrap(true);
        label->setFont(copyrightFont);
        connect(label, &QLabel::linkActivated, this, &tAboutDialog::openAboutUrl);
        ui->copyrightLayout->addWidget(label);
    }

    QScroller::grabGesture(ui->scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
}

tAboutDialog::~tAboutDialog() {
    delete ui;
    delete d;
}

void tAboutDialog::on_okButton_clicked() {
    this->close();
}

void tAboutDialog::openAboutUrl(QUrl url) {
    if (url.scheme() == "license") {
        auto jp = new LicenseTextPopover();
        auto* popover = new tPopover(jp);
        popover->setPopoverWidth(-200);
        popover->setPopoverSide(tPopover::Bottom);
        connect(jp, &LicenseTextPopover::done, popover, &tPopover::dismiss);
        connect(popover, &tPopover::dismissed, popover, &tPopover::deleteLater);
        connect(popover, &tPopover::dismissed, jp, &LicenseTextPopover::deleteLater);
        popover->show(this->window());
    }
}

void tAboutDialog::resizeEvent(QResizeEvent* event) {
    ui->topWidget->setFixedWidth(ui->centralWidget->width());
}
