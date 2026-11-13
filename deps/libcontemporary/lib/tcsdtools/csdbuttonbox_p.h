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

#ifndef CSDBUTTONBOX_P_H
#define CSDBUTTONBOX_P_H

#include <QWidget>
#include <QPointer>

class CsdButtonBox;
struct CsdButtonBoxPrivate {
    QWidget* parentWidget;
    CsdButtonBox* box;

#ifdef Q_OS_MAC
    void windowShown();
    void windowHidden();

    QPointer<QWidget> closeWidget;
    QPointer<QWidget> minWidget;
    QPointer<QWidget> fsWidget;
#endif
};

#endif // CSDBUTTONBOX_P_H
