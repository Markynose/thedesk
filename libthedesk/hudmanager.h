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
#ifndef HUDMANAGER_H
#define HUDMANAGER_H

#include <QObject>
#include <QVariantMap>

class HudManager : public QObject {
        Q_OBJECT
    public:
        explicit HudManager(QObject* parent = nullptr);

        // Parameters: icon, title, text (a snackbar) or icon, title, value
        // (a level); a snackbar may have an "action" label and "actionId",
        // reported by actionTriggered when the user presses it
        void showHud(QVariantMap parameters);
        void hideHud();
        void triggerAction(QString actionId);

    signals:
        void requestHud(QVariantMap parameters);
        void requestHideHud();
        void actionTriggered(QString actionId);
};

#endif // HUDMANAGER_H
