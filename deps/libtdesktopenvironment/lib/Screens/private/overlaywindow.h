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
#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <QDialog>
#include <QPixmap>

QT_BEGIN_NAMESPACE
namespace Ui {
    class OverlayWindow;
}
QT_END_NAMESPACE

class OverlayWindow : public QWidget {
        Q_OBJECT

    public:
        OverlayWindow(QWidget* parent = nullptr);
        ~OverlayWindow();

        void setVisible(bool visible) override;

    private:
        Ui::OverlayWindow* ui;
        QPixmap backdrop; // the screen as it was, under the scrim (X11 without a compositor)

        // QWidget interface
    protected:
        void showEvent(QShowEvent* event);
        void paintEvent(QPaintEvent* event);
};
#endif // OVERLAYWINDOW_H
