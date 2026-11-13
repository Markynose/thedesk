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

#ifndef TVIRTUALKEYBOARD_H
#define TVIRTUALKEYBOARD_H

#include "libcontemporary_global.h"
#include <QObject>

#ifdef T_OS_UNIX_NOT_MAC
#include <QDBusInterface>
#include <QDBusConnectionInterface>
#endif

class LIBCONTEMPORARY_EXPORT tVirtualKeyboard : public QObject
{
    Q_OBJECT
public:
    static tVirtualKeyboard* instance();

Q_SIGNALS:
    void keyboardVisibleChanged(bool isVisible);

public Q_SLOTS:
    int height();
    void showKeyboard();
    void hideKeyboard();
    bool isKeyboardRunning();

    bool keyboardVisible();
    void setKeyboardType(QString type);
    void setPredictive(bool predictive);
    void setHidden(bool hidden);
    void setSensitive(bool sensitive);
    void setAutoUppercase(bool autoUpperCase);
    void setEnterKeyType(QString type);

private:
    tVirtualKeyboard();

#ifdef T_OS_UNIX_NOT_MAC
    QDBusInterface* keyboardInterface;
#endif
};

#endif // TVIRTUALKEYBOARD_H
