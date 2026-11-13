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

#include "tvirtualkeyboard.h"

tVirtualKeyboard::tVirtualKeyboard() : QObject(nullptr) {
#ifdef T_OS_UNIX_NOT_MAC
    keyboardInterface = new QDBusInterface("org.thesuite.tskbd", "/org/thesuite/tskbd", "org.thesuite.tskbd");
    QDBusConnection::sessionBus().connect("org.thesuite.tskbd", "/org/thesuite/tskbd", "org.thesuite.tskbd", "keyboardVisibleChanged", this, SIGNAL(keyboardVisibleChanged(bool)));
#endif
}

tVirtualKeyboard* tVirtualKeyboard::instance() {
    static tVirtualKeyboard* appInst;
    if (appInst == nullptr) {
        appInst = new tVirtualKeyboard;
    }

    return appInst;
}

bool tVirtualKeyboard::isKeyboardRunning() {
#ifdef T_OS_UNIX_NOT_MAC
    return QDBusConnection::sessionBus().interface()->registeredServiceNames().value().contains("org.thesuite.tskbd");
#else
    return false;
#endif
}

void tVirtualKeyboard::hideKeyboard() {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::Block, "hideKeyboard");
    }
#endif
}

void tVirtualKeyboard::showKeyboard() {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::Block, "showKeyboard");
    }
#endif
}

int tVirtualKeyboard::height() {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        return keyboardInterface->call(QDBus::Block, "height").arguments().first().toInt();
    } else {
        return -1;
    }
#else
    return -1;
#endif
}

bool tVirtualKeyboard::keyboardVisible() {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        return keyboardInterface->call(QDBus::Block, "keyboardVisible").arguments().first().toBool();
    } else {
        return false;
    }
#else
    return false;
#endif
}

void tVirtualKeyboard::setKeyboardType(QString type) {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::NoBlock, "setKeyboardType", type);
    }
#endif
}

void tVirtualKeyboard::setPredictive(bool predictive) {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::NoBlock, "setPredictive", predictive);
    }
#endif
}

void tVirtualKeyboard::setHidden(bool hidden) {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::NoBlock, "setHidden", hidden);
    }
#endif
}

void tVirtualKeyboard::setSensitive(bool sensitive) {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::NoBlock, "setSensitive", sensitive);
    }
#endif
}

void tVirtualKeyboard::setAutoUppercase(bool autoUpperCase) {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::NoBlock, "setAutoUpperCase", autoUpperCase);
    }
#endif
}

void tVirtualKeyboard::setEnterKeyType(QString type) {
#ifdef T_OS_UNIX_NOT_MAC
    if (isKeyboardRunning()) {
        keyboardInterface->call(QDBus::NoBlock, "setEnterKeyType", type);
    }
#endif
}
