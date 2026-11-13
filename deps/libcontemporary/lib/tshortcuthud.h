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

#ifndef TSHORTCUTHUD_H
#define TSHORTCUTHUD_H

#include "libcontemporary_global.h"
#include <QWidget>
#include <QShortcut>
#include <QPaintEvent>
#include <QPainter>

namespace Ui {
    class tShortcutHud;
}

class LIBCONTEMPORARY_EXPORT tShortcutHud : public QWidget
{
        Q_OBJECT

    public:
        explicit tShortcutHud(QWidget *parent = nullptr);
        ~tShortcutHud();

        enum ShortcutHudSide {
            Leading,
            Trailing
        };

        class ShortcutGroup {
            public:
                ShortcutGroup();
                ShortcutGroup(QList<QShortcut*> shortcuts, QString shortcutText, ShortcutHudSide side = Leading);

                void addShortcut(QShortcut* shortcut);
                void setText(QString text);
                void setSize(ShortcutHudSide side);

                QList<QShortcut*> shortcuts();
                QString shortcutText();
                ShortcutHudSide side();

            private:
                QList<QShortcut*> scs;
                QString st;
                ShortcutHudSide sd;
        };

    public slots:
        void newShortcut(QShortcut* shortcut, QString shortcutText, ShortcutHudSide side = Leading);
        void newShortcut(ShortcutGroup group);

    private:
        Ui::tShortcutHud *ui;

        QPixmap getKeyIcon(QString key);

        void resizeToParent();
        void paintEvent(QPaintEvent* paintEvent);
        bool eventFilter(QObject* watched, QEvent* event);
};

#endif // TSHORTCUTHUD_H
