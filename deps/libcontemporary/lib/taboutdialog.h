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

#ifndef TABOUTDIALOG_H
#define TABOUTDIALOG_H

#include "libcontemporary_global.h"
#include <QDialog>

namespace Ui {
    class tAboutDialog;
}

struct tAboutDialogPrivate;
class LIBCONTEMPORARY_EXPORT tAboutDialog : public QDialog {
        Q_OBJECT

    public:
        explicit tAboutDialog(QWidget* parent = nullptr);
        ~tAboutDialog();

    private slots:
        void on_okButton_clicked();

    private:
        Ui::tAboutDialog* ui;
        tAboutDialogPrivate* d;

        void openAboutUrl(QUrl url);

        // QWidget interface
    protected:
        void resizeEvent(QResizeEvent* event);
};

#endif // TABOUTDIALOG_H
