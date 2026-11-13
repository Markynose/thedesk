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

#include <QWindow>
#import <AppKit/AppKit.h>

namespace CsdButtonBoxObjcMac {
    QWidget* standardWindowButton(NSWindowButton windowButton, NSWindowStyleMask styleMask, QWidget* parent) {
        NSButton* button = [NSWindow standardWindowButton:windowButton forStyleMask:styleMask];
        auto widget = new QWidget(parent);
        widget->setFixedSize(14, 16);

        NSView* cv = reinterpret_cast<NSView*>(widget->winId());
        [cv addSubview:button];

        return widget;
    }
}

void CsdButtonBoxPrivate::windowHidden() {
    if (closeWidget) {
        box->ui->macWindowControlsLayout->removeWidget(closeWidget);
        box->ui->macWindowControlsLayout->removeWidget(minWidget);
        box->ui->macWindowControlsLayout->removeWidget(fsWidget);

        closeWidget->deleteLater();
        minWidget->deleteLater();
        fsWidget->deleteLater();

        closeWidget = nullptr;
        minWidget = nullptr;
        fsWidget = nullptr;
    }
}

void CsdButtonBoxPrivate::windowShown() {
    if (!closeWidget) {
        auto flags = this->parentWidget->window()->windowFlags();
        NSWindowStyleMask styleMask = NSWindowStyleMaskFullSizeContentView|NSWindowStyleMaskResizable|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable;

        closeWidget = CsdButtonBoxObjcMac::standardWindowButton(NSWindowCloseButton, styleMask, this->parentWidget);
        box->ui->macWindowControlsLayout->addWidget(closeWidget);

        if (flags & Qt::WindowMinimizeButtonHint && flags & Qt::WindowMaximizeButtonHint) {
            minWidget = CsdButtonBoxObjcMac::standardWindowButton(NSWindowMiniaturizeButton, styleMask, this->parentWidget);
            box->ui->macWindowControlsLayout->addWidget(minWidget);

            fsWidget = CsdButtonBoxObjcMac::standardWindowButton(NSWindowZoomButton, styleMask, this->parentWidget);
            box->ui->macWindowControlsLayout->addWidget(fsWidget);
        }
    }
}

void CsdButtonBox::setupMacOs() {
    ui->closeButton->setVisible(false);
    ui->maxButton->setVisible(false);
    ui->minButton->setVisible(false);

    ui->macWindowControlsLayout->setContentsMargins(6, 0, 6, 0);
    ui->macWindowControlsLayout->setSpacing(6);

    //BUG: The following line causes the bug
    //windowShown();
}
