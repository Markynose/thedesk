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

#ifndef TMESSAGEBOX_H
#define TMESSAGEBOX_H

#include "libcontemporary_global.h"
#include <QCoroTask>
#include <QMessageBox>

class tMessageBox;
class tMessageBoxBackend;

class LIBCONTEMPORARY_EXPORT tMessageBoxButton final : public QObject {
        Q_OBJECT
    public:
        explicit tMessageBoxButton(QObject* parent) :
            QObject(parent) {}
    signals:
        void buttonPressed(bool checkboxChecked);
};

struct tMessageBoxPrivate;
class LIBCONTEMPORARY_EXPORT tMessageBox final : public QObject {
        Q_OBJECT
    public:
        explicit tMessageBox(QWidget* parent = nullptr);
        ~tMessageBox();

        void setIcon(QMessageBox::Icon style);
        void setIcon(const QIcon& icon);

        tMessageBoxButton* addStandardButton(QMessageBox::StandardButton buttonType);
        tMessageBoxButton* addButton(const QString& label, QMessageBox::ButtonRole buttonStyle);

        void setDefaultButton(tMessageBoxButton* button);
        void setTitleBarText(const QString& text);
        void setMessageText(const QString& text);
        void setInformativeText(const QString& text);
        void setDetailedText(const QString& text);
        void setCheckboxText(const QString& text);

        void show(bool deleteOnClose = false);
        void exec(bool deleteOnClose = false);
        QCoro::Task<tMessageBoxButton*> presentAsync(bool* checkboxChecked = nullptr);

    private:
        tMessageBoxPrivate* d;
        void initBackend(tMessageBoxBackend& backend);
};

#endif // TMESSAGEBOX_H
