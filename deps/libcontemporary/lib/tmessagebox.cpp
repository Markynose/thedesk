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

#include "tmessagebox.h"
#include "private/tmessageboxbackend.h"

#include "tapplication.h"
#include "torderedmap.h"
#include <QCoroSignal>
#include <QPainter>

struct tMessageBoxPrivate {
        QMessageBox::Icon iconStyle = QMessageBox::NoIcon;
        QIcon icon;
        tOrderedMap<tMessageBoxButton*, tMessageBoxButtonInfo*> buttonMap;
        tMessageBoxButton* defaultButton{};

        QString titleText;
        QString messageText;
        QString informativeText;
        QString detailedText;
        QString checkboxText;

        tMessageBoxButton* pressedButton = nullptr;
        bool checkboxChecked = false;
};

tMessageBox::tMessageBox(QWidget* parent) :
    QObject(parent) {
    d = new tMessageBoxPrivate;
}

tMessageBox::~tMessageBox() {
    delete d;
}

void tMessageBox::setIcon(QMessageBox::Icon style) {
    d->iconStyle = style;
}

void tMessageBox::setIcon(const QIcon& icon) {
    d->icon = icon;
}

tMessageBoxButton* tMessageBox::addStandardButton(QMessageBox::StandardButton buttonType) {
    auto* button = new tMessageBoxButton(this);
    auto* info = new tMessageBoxButtonInfo(button);
    info->buttonType = buttonType;
    d->buttonMap.append(button, info);

    connect(button, &tMessageBoxButton::buttonPressed, this, [this, button](bool checkboxChecked) {
        d->pressedButton = button;
        d->checkboxChecked = checkboxChecked;
    });
    return button;
}

tMessageBoxButton* tMessageBox::addButton(const QString& label, QMessageBox::ButtonRole buttonStyle) {
    auto* button = new tMessageBoxButton(this);
    auto* info = new tMessageBoxButtonInfo(button);
    info->label = label;
    info->buttonStyle = buttonStyle;
    d->buttonMap.append(button, info);

    connect(button, &tMessageBoxButton::buttonPressed, this, [this, button](bool checkboxChecked) {
        d->pressedButton = button;
        d->checkboxChecked = checkboxChecked;
    });

    return button;
}

void tMessageBox::setDefaultButton(tMessageBoxButton* button) {
    if (!d->buttonMap.contains(button)) {
        // bug in caller; pretend this didn't happen
        return;
    }

    if (d->defaultButton) {
        d->buttonMap.value(d->defaultButton)->isDefault = false;
    }

    d->defaultButton = button;
    d->buttonMap.value(button)->isDefault = true;
}

void tMessageBox::setTitleBarText(const QString& text) {
    d->titleText = text;
}

void tMessageBox::setMessageText(const QString& text) {
    d->messageText = text;
}

void tMessageBox::setInformativeText(const QString& text) {
    d->informativeText = text;
}

void tMessageBox::setDetailedText(const QString& text) {
    d->detailedText = text;
}

void tMessageBox::setCheckboxText(const QString& text) {
    d->checkboxText = text;
}

void tMessageBox::initBackend(tMessageBoxBackend& backend) {
    if (d->buttonMap.isEmpty()) {
        auto* button = new tMessageBoxButton(this);
        auto* info = new tMessageBoxButtonInfo(button);
        info->buttonType = QMessageBox::StandardButton::Ok;
        d->buttonMap.append(button, info);
    }

    backend.init(d->iconStyle,
        d->icon,
        d->titleText,
        d->messageText,
        d->informativeText,
        d->detailedText,
        d->checkboxText,
        d->buttonMap);
    backend.open(qobject_cast<QWidget*>(parent()));
}

void tMessageBox::show(bool deleteOnClose) {
    auto* backend = new tMessageBoxBackend(this);
    initBackend(*backend);
    if (deleteOnClose) {
        connect(backend, &tMessageBoxBackend::canBeDestroyed, this, &tMessageBox::deleteLater);
    }
}

void tMessageBox::exec(bool deleteOnClose) {
    QEventLoop eventLoop;
    tMessageBoxBackend backend(nullptr);
    initBackend(backend);
    connect(&backend, &tMessageBoxBackend::canBeDestroyed, &eventLoop, &QEventLoop::quit);

    if (deleteOnClose) {
        connect(&backend, &tMessageBoxBackend::canBeDestroyed, this, &tMessageBox::deleteLater);
    }

    eventLoop.exec();
}

QCoro::Task<tMessageBoxButton*> tMessageBox::presentAsync(bool* checkboxChecked) {
    tMessageBoxBackend backend(nullptr);
    initBackend(backend);

    co_await qCoro(&backend, &tMessageBoxBackend::canBeDestroyed);

    tMessageBoxButton* button = d->pressedButton;
    bool checked = d->checkboxChecked;
    if (checkboxChecked) *checkboxChecked = checked;
    co_return button;
}
