/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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
#ifndef FILEASSOCIATIONSELECT_H
#define FILEASSOCIATIONSELECT_H

#include <QDialog>
#include <QMimeType>
#include <QListWidgetItem>
#include <Applications/application.h>

namespace Ui {
    class FileAssociationSelect;
}

struct FileAssociationSelectPrivate;
class FileAssociationSelect : public QDialog {
        Q_OBJECT

    public:
        explicit FileAssociationSelect(QUrl file, QString mimeType, QList<ApplicationPointer> availableApplications, QWidget* parent = nullptr);
        ~FileAssociationSelect();

        ApplicationPointer selectedApplication();
        bool shouldSetDefault();

    private slots:
        void on_dismissButton_clicked();

        void on_stackedWidget_switchingFrame(int frame);

        void on_allAppsTitle_backButtonClicked();

        void on_chooseAppNoAssoc_clicked();

        void on_showAllAppsMultiApp_clicked();

        void on_multiAppsList_itemActivated(QListWidgetItem* item);

        void on_allAppsList_itemActivated(QListWidgetItem* item);

        void on_dismissButtonMulti_clicked();

        void on_lastUseButton_clicked();

        void on_lastUseButtonNoAssoc_clicked();

    private:
        Ui::FileAssociationSelect* ui;
        FileAssociationSelectPrivate* d;

        void useApp(QString desktopEntry);
};

#endif // FILEASSOCIATIONSELECT_H
