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
#include "setdefaultpopover.h"
#include "ui_setdefaultpopover.h"

#include <QMimeType>
#include <QMimeDatabase>

SetDefaultPopover::SetDefaultPopover(QString mimeType, ApplicationPointer app, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SetDefaultPopover) {
    ui->setupUi(this);

    ui->titleLabel->setBackButtonShown(true);
    ui->setDefaultButton->setText(tr("Always use %1").arg(QLocale().quoteString(app->getProperty("Name").toString())));

    if (mimeType.startsWith("x-scheme-handler/")) {
        ui->descriptionLabel->setText(tr("When opening %1 links, do you always want to use %2?").arg(QLocale().quoteString(mimeType.mid(mimeType.indexOf("/") + 1)), QLocale().quoteString(app->getProperty("Name").toString())));
    } else {
        QMimeDatabase db;
        QMimeType mt = db.mimeTypeForName(mimeType);
        ui->descriptionLabel->setText(tr("When opening files of type %1, do you always want to use %2?").arg(QLocale().quoteString(mt.comment()), QLocale().quoteString(app->getProperty("Name").toString())));
    }
}

SetDefaultPopover::~SetDefaultPopover() {
    delete ui;
}

void SetDefaultPopover::on_setDefaultButton_clicked() {
    emit accepted(true);
}

void SetDefaultPopover::on_dontSetDefaultButton_clicked() {
    emit accepted(false);
}

void SetDefaultPopover::on_titleLabel_backButtonClicked() {
    emit rejected();
}
