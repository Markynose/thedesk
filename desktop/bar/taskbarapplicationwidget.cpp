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
#include "taskbarapplicationwidget.h"
#include <m3themedicon.h>

#include <Wm/desktopwm.h>
#include <libcontemporary_global.h>

#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <m3motion.h>
#include <m3scheme.h>
#include <Wm/desktopwm.h>
#include <actionquickwidget.h>
#include <barmanager.h>
#include <private/quickwidgetcontainer.h>
#include <statemanager.h>

struct TaskbarApplicationWidgetPrivate {
        ApplicationPointer app;
        uint desktop;

        ActionQuickWidget* windowListMenu;
        QuickWidgetContainer* windowListMenuContainer;

        QList<DesktopWmWindowPtr> trackedWindows;
        QMap<DesktopWmWindowPtr, QAction*> actions;

        M3SpringAnimation* checked;
};

TaskbarApplicationWidget::TaskbarApplicationWidget(QString desktopEntry, uint desktop, QWidget* parent) :
    QPushButton(parent) {
    d = new TaskbarApplicationWidgetPrivate();
    d->app = ApplicationPointer(new Application(desktopEntry));
    d->desktop = desktop;

    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    this->setFixedWidth(44);
    this->setCheckable(true);
    this->setAttribute(Qt::WA_Hover);

    // M3E toggle icon button: round off, CornerMedium and SecondaryContainer on
    d->checked = new M3SpringAnimation(this);
    d->checked->setToken(M3Motion::SpatialFast);
    d->checked->snapTo(0.0);
    connect(d->checked, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        d->checked->animateTo(on ? 1.0 : 0.0);
    });

    connect(DesktopWm::instance(), &DesktopWm::windowRemoved, this, [=](DesktopWmWindowPtr window) {
        removeTrackedWindow(window);
    });
    connect(DesktopWm::instance(), &DesktopWm::activeWindowChanged, this, &TaskbarApplicationWidget::updateActive);

    connect(this, &TaskbarApplicationWidget::clicked, this, [=] {
        if (d->trackedWindows.count() == 1) {
            d->trackedWindows.first()->activate();
        } else {
            if (!d->windowListMenuContainer->isShowing()) d->windowListMenuContainer->showContainer();
        }
        updateActive();
    });
    this->setIconSize(QSize(30, 30));

    d->windowListMenuContainer = new QuickWidgetContainer(this);
    d->windowListMenu = new ActionQuickWidget(d->windowListMenuContainer);
    d->windowListMenuContainer->setQuickWidget(d->windowListMenu);
    //    connect(d->windowListMenu, &ActionQuickWidget::done, d->menuContainer, &QuickWidgetContainer::hideContainer);

    updateIcon();
}

TaskbarApplicationWidget::~TaskbarApplicationWidget() {
    delete d;
}

void TaskbarApplicationWidget::trackWindow(DesktopWmWindowPtr window) {
    if (d->trackedWindows.contains(window)) return;

    QAction* action = new QAction(this);
    action->setText(window->title());

    connect(window, &DesktopWmWindow::titleChanged, action, [=] {
        action->setText(window->title());
    });
    connect(action, &QAction::triggered, window, [=] {
        window->activate();
    });

    d->actions.insert(window, action);
    d->windowListMenu->addAction(action);
    d->trackedWindows.append(window);
    updateIcon();
    updateActive();
}

void TaskbarApplicationWidget::removeTrackedWindow(DesktopWmWindowPtr window) {
    if (!d->trackedWindows.contains(window)) return;

    window->disconnect(this);
    d->trackedWindows.removeOne(window);
    QAction* action = d->actions.take(window);
    d->windowListMenu->removeAction(action);
    action->deleteLater();
    updateIcon();
    updateActive();

    if (d->trackedWindows.isEmpty()) emit windowsRemoved();
}

QList<DesktopWmWindowPtr> TaskbarApplicationWidget::trackedWindows() {
    return d->trackedWindows;
}

void TaskbarApplicationWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const double on = qBound(0.0, d->checked->currentValue().toDouble(), 1.0);
    QRectF r(0, 0, 40, 40);
    r.moveCenter(QRectF(this->rect()).center());
    const double radius = 20 + (12 - 20) * d->checked->currentValue().toDouble();
    QPainterPath shape;
    shape.addRoundedRect(r, radius, radius);

    QColor container = s.secondaryContainer;
    container.setAlphaF(float(on));
    painter.fillPath(shape, container);
    QStyleOption option;
    option.initFrom(this);
    double layer = this->underMouse() ? 0.08 : 0;
    if (this->isDown()) layer = 0.10;
    if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_KeyboardFocusChange)) layer = qMax(layer, 0.10);
    if (layer > 0) {
        QColor state = on > 0.5 ? s.onSecondaryContainer : s.onSurfaceVariant;
        state.setAlphaF(float(layer));
        painter.fillPath(shape, state);
    }

    QRect iconRect(QPoint(0, 0), this->iconSize());
    iconRect.moveCenter(r.center().toPoint());
    this->icon().paint(&painter, iconRect);
}

void TaskbarApplicationWidget::updateIcon() {
    // Themed icons (m3themedicon.h) where it's on
    if (d->app->isValid()) {
        this->setIcon(M3ThemedIcon::icon(d->app->icon()));
    } else if (!d->trackedWindows.isEmpty()) {
        this->setIcon(M3ThemedIcon::icon(d->trackedWindows.first()->icon()));
    } else {
        this->setIcon(M3ThemedIcon::icon(QIcon::fromTheme("generic-app")));
    }

    emit iconChanged();
}

void TaskbarApplicationWidget::updateActive() {
    this->setChecked(d->trackedWindows.contains(DesktopWm::activeWindow()));
}

void TaskbarApplicationWidget::contextMenuEvent(QContextMenuEvent* event) {
    auto menu = new QMenu(this);
    if (d->trackedWindows.count() == 1) {
        auto win = d->trackedWindows.constFirst();
        menu->addSection(tr("For %1").arg(QLocale().quoteString(win->title())));

        QMenu* desktopsMenu = new QMenu(menu);
        desktopsMenu->setTitle(tr("Move to desktop"));

        auto desktopsGroup = new QActionGroup(desktopsMenu);
        desktopsGroup->setExclusive(true);

        auto allDesktopsAction = desktopsMenu->addAction(tr("All Desktops"), [win] {
            win->moveToDesktop(UINT_MAX);
        });
        desktopsGroup->addAction(allDesktopsAction);
        allDesktopsAction->setCheckable(true);
        if (win->desktop() == UINT_MAX) allDesktopsAction->setChecked(true);
        desktopsMenu->addSeparator();

        for (auto i = 0; i < DesktopWm::desktops().count() - 1; i++) {
            auto desktopName = DesktopWm::desktops().at(i);
            auto action = desktopsMenu->addAction(desktopName, [win, i] {
                win->moveToDesktop(i);
            });
            desktopsGroup->addAction(action);
            action->setCheckable(true);
            if (i == win->desktop()) action->setChecked(true);
        }
        desktopsMenu->addSeparator();
        desktopsMenu->addAction(QIcon::fromTheme("list-add"), tr("New Desktop"), [win] {
            win->moveToDesktop(DesktopWm::desktops().count() - 1);
        });
        menu->addMenu(desktopsMenu);

        menu->addAction(QIcon::fromTheme("window-close"), tr("Close"), this, [win] {
            win->close();
        });
    } else {
        if (d->app->isValid()) {
            menu->addSection(tr("For %n %1 windows", nullptr, d->trackedWindows.count()).arg(QLocale().quoteString(d->app->getProperty("Name").toString())));
        } else {
            menu->addSection(tr("For %n windows", nullptr, d->trackedWindows.count()));
        }
        menu->addAction(QIcon::fromTheme("application-exit"), tr("Close All"), this, [this] {
            for (auto win : d->trackedWindows) {
                win->close();
            }
        });
    }

    auto locker = StateManager::barManager()->acquireLock();
    connect(menu, &QMenu::aboutToHide, this, [locker, menu] {
        locker->unlock();
        menu->deleteLater();
    });

    menu->popup(event->globalPos());
}
