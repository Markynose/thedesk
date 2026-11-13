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
#ifndef KEYGRAB_H
#define KEYGRAB_H

#include <QObject>

// Every shortcut, as it fires, by its setting name ("gatewayOpen", ...): for
// things that teach the shortcuts (onboarding's tips)
class KeyGrabHub : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;

    signals:
        void activated(QString settingName);
};

struct KeyGrabPrivate;
class KeyGrab : public QObject {
        Q_OBJECT
    public:
        explicit KeyGrab(QKeySequence seq, QObject* parent = nullptr);
        explicit KeyGrab(QKeySequence defaultSeq, QString settingName, QObject* parent = nullptr);
        ~KeyGrab();

        void pause();
        void resume();

        static KeyGrabHub* hub();
        // Does what the shortcut does, as if it had been pressed; false if
        // there is no shortcut by that name
        static bool trigger(const QString& settingName);

    signals:
        void activated();

    private:
        KeyGrabPrivate* d;

        void init();
        void loadSetting();
};

#endif // KEYGRAB_H
