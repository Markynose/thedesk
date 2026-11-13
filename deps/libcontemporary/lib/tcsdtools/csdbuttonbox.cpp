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
#include "csdbuttonbox.h"
#include "csdbuttonbox_p.h"
#include "ui_csdbuttonbox.h"

#include "libcontemporary_global.h"
#include "tcsdtools.h"
#include <QPainter>

CsdButtonBox::CsdButtonBox(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::CsdButtonBox) {
    ui->setupUi(this);
    d = new CsdButtonBoxPrivate();
    d->box = this;

    d->parentWidget = parent;
    parent->installEventFilter(this);

    ui->minButton->setIconSize(QSize(24, 24));
    ui->maxButton->setIconSize(QSize(24, 24));
    ui->closeButton->setIconSize(QSize(24, 24));

    if (tCsdGlobal::windowControlsEdge() == tCsdGlobal::Left) {
        static_cast<QBoxLayout*>(this->layout())->setDirection(QBoxLayout::RightToLeft);
    }

    connect(tCsdGlobal::instance(), &tCsdGlobal::csdsEnabledChanged, this, &CsdButtonBox::csdsEnabledChanged);
    csdsEnabledChanged(tCsdGlobal::csdsEnabled());
    updateButtons();

#ifdef Q_OS_MAC
    setupMacOs();
#endif
}

CsdButtonBox::~CsdButtonBox() {
    delete d;
    delete ui;
}

bool CsdButtonBox::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        if (d->parentWidget->window()->isMaximized()) {
            ui->maxButton->setIcon(QIcon(":/tcsdtools/res.svg"));
        } else {
            ui->maxButton->setIcon(QIcon(":/tcsdtools/max.svg"));
        }
    }

#ifdef Q_OS_MAC
    if (event->type() == QEvent::Hide) {
        d->windowHidden();
    } else if (event->type() == QEvent::Show) {
        d->windowShown();
    } else if (event->type() == QEvent::ActivationChange || event->type() == QEvent::WindowStateChange) {
        d->windowHidden();
        d->windowShown();
    }
#endif
    return false;
}

void CsdButtonBox::updateButtons() {
#ifndef Q_OS_MAC
    auto flags = this->window()->windowFlags();
    if (flags & Qt::WindowMinimizeButtonHint && flags & Qt::WindowMaximizeButtonHint) {
        ui->maxButton->setVisible(true);
        ui->minButton->setVisible(true);

        ui->maxButton->setEnabled(flags & Qt::WindowMaximizeButtonHint);
        ui->minButton->setEnabled(flags & Qt::WindowMinimizeButtonHint);
    } else {
        ui->maxButton->setVisible(false);
        ui->minButton->setVisible(false);
    }
#endif
}

void CsdButtonBox::on_closeButton_clicked() {
    d->parentWidget->window()->close();
}

void CsdButtonBox::on_maxButton_clicked() {
    if (d->parentWidget->window()->isMaximized()) {
        d->parentWidget->window()->showNormal();
    } else {
        d->parentWidget->window()->showMaximized();
    }
}

void CsdButtonBox::on_minButton_clicked() {
    d->parentWidget->window()->showMinimized();
}

void CsdButtonBox::csdsEnabledChanged(bool enabled) {
    this->setVisible(enabled);
}
