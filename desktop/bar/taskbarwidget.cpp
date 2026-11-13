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
#include "taskbarwidget.h"
#include "ui_taskbarwidget.h"

#include "taskbardesktopwidget.h"
#include <Applications/application.h>
#include <Gestures/gesturedaemon.h>
#include <QPushButton>
#include <QTimer>
#include <Wm/desktopwm.h>
#include <libcontemporary_global.h>
#include <tsettings.h>

namespace {
    // Dynamic desktops (always exactly one empty desktop at the end) need a
    // window manager that honours _NET_NUMBER_OF_DESKTOPS requests. Tiling WMs
    // like bspwm manage a fixed set of desktops themselves: set
    // [Desktops] dynamic=false for those.
    bool dynamicDesktops() {
        static tSettings settings;
        return settings.value(QStringLiteral("Desktops/dynamic")).toBool();
    }

    int shownDesktopCount() {
        int count = DesktopWm::desktops().count();
        return dynamicDesktops() ? count - 1 : count;
    }
} // namespace

struct TaskbarWidgetPrivate {
        QList<TaskbarDesktopWidget*> desktopWidgets;
};

TaskbarWidget::TaskbarWidget(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::TaskbarWidget) {
    ui->setupUi(this);

    d = new TaskbarWidgetPrivate();

    //    connect(DesktopWm::instance(), &DesktopWm::windowAdded, this, &TaskbarWidget::addWindow);
    //    connect(DesktopWm::instance(), &DesktopWm::windowRemoved, this, &TaskbarWidget::removeWindow);
    //    connect(DesktopWm::instance(), &DesktopWm::activeWindowChanged, this, &TaskbarWidget::activeWindowChanged);

    //    for (DesktopWmWindowPtr window : DesktopWm::openWindows()) {
    //        this->addWindow(window);
    //    }

    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, this, &TaskbarWidget::updateDesktop);
    connect(DesktopWm::instance(), &DesktopWm::desktopCountChanged, this, &TaskbarWidget::updateDesktop);

    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, this, &TaskbarWidget::normaliseDesktops);
    connect(DesktopWm::instance(), &DesktopWm::desktopCountChanged, this, &TaskbarWidget::normaliseDesktops);
    connect(DesktopWm::instance(), &DesktopWm::windowAdded, this, &TaskbarWidget::addWindow);
    connect(DesktopWm::instance(), &DesktopWm::windowRemoved, this, &TaskbarWidget::removeWindow);

    // TODO: Move to window manager
    connect(GestureDaemon::instance(), &GestureDaemon::gestureBegin, this, [=](GestureInteractionPtr gesture) {
        if (gesture->isValidInteraction(GestureTypes::Swipe, GestureTypes::Left, 4)) {
            connect(gesture.data(), &GestureInteraction::interactionEnded, this, [=] {
                if (gesture->extrapolatePercentage(100) > 0.7 && DesktopWm::currentDesktop() != DesktopWm::desktops().length() - 1) {
                    DesktopWm::setCurrentDesktop(DesktopWm::currentDesktop() + 1);
                }
            });
        } else if (gesture->isValidInteraction(GestureTypes::Swipe, GestureTypes::Right, 4)) {
            connect(gesture.data(), &GestureInteraction::interactionEnded, this, [=] {
                if (gesture->extrapolatePercentage(100) > 0.7 && DesktopWm::currentDesktop() != 0) {
                    DesktopWm::setCurrentDesktop(DesktopWm::currentDesktop() - 1);
                }
            });
        }
    });

    ui->lastDesktopButton->setIconSize(SC_DPI_T(QSize(24, 24), QSize));
    // The "new desktop" button is the spare empty desktop of dynamic mode
    ui->lastDesktopButton->setVisible(DesktopWm::supportsSetNumDesktops() && dynamicDesktops());

    normaliseDesktops();
    updateDesktop();
}

TaskbarWidget::~TaskbarWidget() {
    delete d;
    delete ui;
    d = nullptr;
}

void TaskbarWidget::addWindow(DesktopWmWindowPtr window) {
    connect(window, &DesktopWmWindow::desktopChanged, this, &TaskbarWidget::normaliseDesktops);

    // Race condition?
    QTimer::singleShot(500, this, [=] {
        normaliseDesktops();
    });
}

void TaskbarWidget::removeWindow(DesktopWmWindowPtr window) {
    // Race condition?
    QTimer::singleShot(500, this, [=] {
        normaliseDesktops();
    });
}

void TaskbarWidget::activeWindowChanged() {
}

void TaskbarWidget::normaliseDesktops() {
    // Ensure there is a taskbar desktop widget for every desktop - 1
    for (int i = 0; i < shownDesktopCount(); i++) {
        if (d->desktopWidgets.count() == i) {
            // Add another widget
            TaskbarDesktopWidget* widget = new TaskbarDesktopWidget(i);
            ui->desktopsLayout->addWidget(widget);
            d->desktopWidgets.append(widget);
        }
    }

    // Remove any extraenous desktop widgets
    while (d->desktopWidgets.count() > shownDesktopCount()) {
        TaskbarDesktopWidget* widget = d->desktopWidgets.takeLast();
        ui->desktopsLayout->removeWidget(widget);
        widget->deleteLater();
    }

    if (!dynamicDesktops()) return;

    // Go through each desktop to find any that need to be removed
    for (int i = 0; i < DesktopWm::desktops().count() - 1; i++) {
        if (DesktopWm::currentDesktop() == static_cast<uint>(i)) continue;

        bool haveWindows = false;
        for (auto window : DesktopWm::windowsOnDesktop(i)) {
            if (window->desktop() != UINT_MAX) haveWindows = true;
        }
        if (haveWindows) continue;

        // This desktop needs to be removed
        // Shuffle all of the windows from the desktops in front down
        for (int j = i + 1; j < DesktopWm::desktops().count() - 1; j++) {
            d->desktopWidgets.at(j)->moveDesktop(j - 1);
        }

        // Remove the last desktop. Don't recurse here: desktopCountChanged
        // re-runs this once the WM has actually applied it. (Recursing
        // immediately loops forever on WMs that ignore the request.)
        DesktopWm::setNumDesktops(DesktopWm::desktops().count() - 1);
        return;
    }

    // Ensure that there is an empty desktop at the end
    bool haveWindows = false;
    for (auto window : DesktopWm::windowsOnDesktop(DesktopWm::desktops().count() - 1)) {
        if (window->desktop() != UINT_MAX) haveWindows = true;
    }
    if (haveWindows) {
        // Add an extra desktop (desktopCountChanged re-runs this afterwards)
        DesktopWm::setNumDesktops(DesktopWm::desktops().count() + 1);
        return;
    }
}

void TaskbarWidget::updateDesktop() {
    // Race condition?
    QTimer::singleShot(500, this, [=] {
        ui->lastDesktopButton->setChecked(DesktopWm::currentDesktop() == DesktopWm::desktops().count() - 1);
    });
}

void TaskbarWidget::on_lastDesktopButton_clicked() {
    DesktopWm::setCurrentDesktop(DesktopWm::desktops().count() - 1);
}

void TaskbarWidget::resizeEvent(QResizeEvent* event) {
    ui->lastDesktopButton->setFixedWidth(this->height());
}
