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
#ifndef TASKBARDESKTOPWIDGET_H
#define TASKBARDESKTOPWIDGET_H

#include <QPushButton>
#include <QWidget>

namespace Ui {
    class TaskbarDesktopWidget;
}

class M3SpringAnimation;

// A button of the desktops' M3E connected button group: tonal segments with
// small inner corners; the selected one turns fully round and Primary, and a
// pressed one widens while its neighbours give way. A dot per app on the
// desktop; no text draws the "add" symbol (the new desktop button)
class DesktopButton : public QPushButton {
        Q_OBJECT

    public:
        explicit DesktopButton(QWidget* parent = nullptr);

        void setAppCount(int count);
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void nextCheckState() override {} // selected follows the current desktop, not clicks
        void paintEvent(QPaintEvent* event) override;

    private:
        M3SpringAnimation* selected; // the shape rounding off
        M3SpringAnimation* tone;     // the colour
        M3SpringAnimation* press;
        int apps = 0;

        QList<DesktopButton*> group() const;
        void updateWidth();
};

class DesktopWmWindow;
typedef QPointer<DesktopWmWindow> DesktopWmWindowPtr;

struct TaskbarDesktopWidgetPrivate;
class TaskbarDesktopWidget : public QWidget {
        Q_OBJECT

    public:
        explicit TaskbarDesktopWidget(uint desktop, QWidget* parent = nullptr);
        ~TaskbarDesktopWidget();

        void moveDesktop(uint newDesktop);
        // Goes in the taskbar's button group; the apps stay in this widget
        DesktopButton* desktopButton();

    private:
        Ui::TaskbarDesktopWidget* ui;
        TaskbarDesktopWidgetPrivate* d;

        void resizeEvent(QResizeEvent* event);

        void windowAdded(DesktopWmWindowPtr window);
        void windowRemoved(DesktopWmWindowPtr window);
        void registerOnDesktop(DesktopWmWindowPtr window);
        void deregisterFromDesktop(DesktopWmWindowPtr window);

        void updateButton();
        void updateDesktop();
        void updateAnimation();
        void finishAnimation();
};

#endif // TASKBARDESKTOPWIDGET_H
