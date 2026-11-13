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

#ifndef TCIRCULARSPINNER_H
#define TCIRCULARSPINNER_H

#include <QWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>
#include <tvariantanimation.h>

class LIBCONTEMPORARY_EXPORT tCircularSpinner : public QWidget
{
        Q_OBJECT
    public:
        explicit tCircularSpinner(QWidget *parent = nullptr);
        ~tCircularSpinner();

        QSize sizeHint() const;
    signals:

    public slots:
        void appear();
        void disappear();

    private:
        void paintEvent(QPaintEvent* event);

        int rotation = 0;
        int arcLength = 16 * 15;
        QTimer* rotationTimer;
};

#endif // TCIRCULARSPINNER_H
