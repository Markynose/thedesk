/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
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
#ifndef TSETTINGSWINDOW_H
#define TSETTINGSWINDOW_H

#include "libcontemporary_global.h"
#include <QDialog>

namespace Ui {
    class tSettingsWindow;
}

class tSettingsPane;
struct tSettingsWindowPrivate;
class LIBCONTEMPORARY_EXPORT tSettingsWindow : public QDialog {
        Q_OBJECT

    public:
        explicit tSettingsWindow(QWidget* parent = nullptr);
        ~tSettingsWindow();

        void appendSection(QString text);
        void appendPane(tSettingsPane* pane);
        void appendSeparator();

        static void addStaticSection(int priority, QString name, QString text);
        static void addStaticPane(int priority, QString sectionName, std::function<tSettingsPane*()> paneGenerator);
        static void openStaticSettingsWindow(QWidget* parent);

    private slots:
        void on_titleLabel_backButtonClicked();

        void on_contentsView_clicked(const QModelIndex& index);

    private:
        Ui::tSettingsWindow* ui;
        tSettingsWindowPrivate* d;

        // QWidget interface
    protected:
        void resizeEvent(QResizeEvent* event);
};

#endif // TSETTINGSWINDOW_H
