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
#include "taskbardesktopwidget.h"

#include <m3motion.h>
#include "ui_taskbardesktopwidget.h"

#include "taskbarapplicationwidget.h"
#include <Applications/application.h>
#include <QPainter>
#include <Wm/desktopwm.h>
#include <tvariantanimation.h>

struct TaskbarDesktopWidgetPrivate {
        QMap<QString, TaskbarApplicationWidget*> applicationWidgets;
        QList<TaskbarApplicationWidget*> applicationWidgetOrder;
        uint desktop;
        QColor color;

        tVariantAnimation* progressAnim;

        // Gruvbox dark accents: blue, aqua, green, yellow, orange, purple
        const QRgb colors[6] = {
            0x458588,
            0x689d6a,
            0x98971a,
            0xd79921,
            0xd65d0e,
            0xb16286};
};

TaskbarDesktopWidget::TaskbarDesktopWidget(uint desktop, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::TaskbarDesktopWidget) {
    ui->setupUi(this);
    d = new TaskbarDesktopWidgetPrivate();
    d->desktop = desktop;

    d->color = QColor::fromRgb(d->colors[desktop % 6]);

    d->progressAnim = new tVariantAnimation();
    d->progressAnim->setStartValue(0.0);
    d->progressAnim->setEndValue(1.0);
    d->progressAnim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    d->progressAnim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    connect(d->progressAnim, &tVariantAnimation::valueChanged, this, &TaskbarDesktopWidget::updateAnimation);
    connect(d->progressAnim, &tVariantAnimation::finished, this, &TaskbarDesktopWidget::finishAnimation);

    if (DesktopWm::currentDesktop() == d->desktop) d->progressAnim->setCurrentTime(d->progressAnim->duration());
    finishAnimation();

    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, this, &TaskbarDesktopWidget::updateDesktop);
    connect(DesktopWm::instance(), &DesktopWm::desktopCountChanged, this, &TaskbarDesktopWidget::updateOverviewButtonIcon);
    connect(DesktopWm::instance(), &DesktopWm::windowAdded, this, &TaskbarDesktopWidget::windowAdded);
    connect(DesktopWm::instance(), &DesktopWm::windowRemoved, this, &TaskbarDesktopWidget::windowRemoved);

    for (DesktopWmWindowPtr window : DesktopWm::openWindows()) {
        this->windowAdded(window);
    }

    updateHighlight(DesktopWm::currentDesktop() == d->desktop ? 1 : 0);
    ui->desktopOverviewButton->setIconSize(SC_DPI_T(QSize(32, 32), QSize));
    updateOverviewButtonIcon();
}

TaskbarDesktopWidget::~TaskbarDesktopWidget() {
    delete d;
    delete ui;
}

void TaskbarDesktopWidget::moveDesktop(uint newDesktop) {
    d->desktop = newDesktop;

    QList<DesktopWmWindowPtr> windowsToMove;
    for (TaskbarApplicationWidget* widget : d->applicationWidgetOrder) {
        windowsToMove.append(widget->trackedWindows());
    }

    for (DesktopWmWindowPtr window : windowsToMove) {
        window->moveToDesktop(newDesktop);
    }
}

void TaskbarDesktopWidget::windowAdded(DesktopWmWindowPtr window) {
    connect(window, &DesktopWmWindow::desktopChanged, this, [this, window] {
        if (window->isOnDesktop(d->desktop) && window->shouldShowInTaskbar()) {
            registerOnDesktop(window);
        } else {
            deregisterFromDesktop(window);
        }
    });

    if (window->isOnDesktop(d->desktop) && window->shouldShowInTaskbar()) registerOnDesktop(window);
}

void TaskbarDesktopWidget::windowRemoved(DesktopWmWindowPtr window) {
    deregisterFromDesktop(window);
}

void TaskbarDesktopWidget::registerOnDesktop(DesktopWmWindowPtr window) {
    QString desktopEntry = QString::number(window->pid());
    if (window->application()) desktopEntry = window->application()->desktopEntry();

    TaskbarApplicationWidget* widget;
    if (d->applicationWidgets.contains(desktopEntry)) {
        widget = d->applicationWidgets.value(desktopEntry);
    } else {
        widget = new TaskbarApplicationWidget(desktopEntry, d->desktop, this);
        widget->setColor(d->color);
        connect(widget, &TaskbarApplicationWidget::iconChanged, this, &TaskbarDesktopWidget::updateOverviewButtonIcon);
        connect(widget, &TaskbarApplicationWidget::windowsRemoved, this, [=] {
            d->applicationWidgets.remove(desktopEntry);
            d->applicationWidgetOrder.removeOne(widget);
            ui->applicationsLayout->removeWidget(widget);
            widget->deleteLater();

            updateOverviewButtonIcon();
        });
        ui->applicationsLayout->addWidget(widget);
        d->applicationWidgets.insert(desktopEntry, widget);
        d->applicationWidgetOrder.append(widget);
    }

    widget->trackWindow(window);
    updateOverviewButtonIcon();
}

void TaskbarDesktopWidget::deregisterFromDesktop(DesktopWmWindowPtr window) {
    QString desktopEntry = QString::number(window->pid());
    if (window->application()) desktopEntry = window->application()->desktopEntry();

    if (d->applicationWidgets.contains(desktopEntry)) {
        TaskbarApplicationWidget* widget = d->applicationWidgets.value(desktopEntry);
        widget->removeTrackedWindow(window);
    }
}

void TaskbarDesktopWidget::updateOverviewButtonIcon() {
    QPixmap pixmap(SC_DPI_T(QSize(32, 32), QSize));
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    for (int i = 0; i < 4; i++) {
        if (d->applicationWidgetOrder.count() <= i) continue;

        QRect boxGeometry;
        boxGeometry.setSize(SC_DPI_T(QSize(16, 16), QSize));
        boxGeometry.moveTopLeft(QPoint(SC_DPI(16) * (i % 2), SC_DPI(16) * (i / 2)));

        QRect geometry;
        geometry.setSize(SC_DPI_T(QSize(14, 14), QSize));
        geometry.moveCenter(boxGeometry.center());

        if (i == 3 && d->applicationWidgetOrder.count() > 4) {
            QFont font = this->font();
            font.setPixelSize(geometry.height());
            painter.setFont(font);

            painter.setPen(this->palette().color(QPalette::ButtonText));
            painter.drawText(geometry, Qt::AlignCenter, QStringLiteral("%1%2").arg(QLocale().positiveSign()).arg(d->applicationWidgetOrder.count() - 3));
        } else {
            QPixmap icon(d->applicationWidgetOrder.at(i)->icon().pixmap(geometry.size()));
            painter.drawPixmap(geometry, icon);
        }
    }

    // Number each desktop with the name the WM gives it (bspwm: 1..10)
    QString name = DesktopWm::desktops().value(d->desktop);
    if (name.isEmpty()) name = QString::number(d->desktop + 1);
    ui->desktopOverviewButton->setText(name);
    ui->desktopOverviewButton->setToolTip(tr("Desktop %1").arg(name));

    // Empty desktop: just the number. Otherwise the number plus app icons.
    ui->desktopOverviewButton->setIcon(d->applicationWidgetOrder.isEmpty() ? QIcon() : QIcon(pixmap));
}

void TaskbarDesktopWidget::updateDesktop() {
    if (DesktopWm::currentDesktop() == d->desktop && d->progressAnim->currentValue().toReal() < 0.5) {
        d->progressAnim->setDirection(tVariantAnimation::Forward);
        d->progressAnim->start();
    } else if (DesktopWm::currentDesktop() != d->desktop && d->progressAnim->currentValue().toReal() > 0.5) {
        d->progressAnim->setDirection(tVariantAnimation::Backward);
        d->progressAnim->start();
    }
}

void TaskbarDesktopWidget::updateAnimation() {
    double progress = d->progressAnim->currentValue().toReal();
    ui->applicationsWidget->setFixedWidth(ui->applicationsWidget->sizeHint().width() * progress);
    // The numbered button always stays visible (collapsing it shifted every
    // following tile); the current desktop is highlighted instead.
    ui->desktopOverviewButton->setFixedWidth(this->height());
    updateHighlight(progress);
}

void TaskbarDesktopWidget::updateHighlight(double progress) {
    // M3 toolbar: the current desktop is the selected (filled) item
    ui->desktopOverviewButton->setCheckable(true);
    ui->desktopOverviewButton->setChecked(progress > 0.5);

    QColor inactive = d->color.darker(170);
    QColor colour;
    colour.setRgbF(inactive.redF() + (d->color.redF() - inactive.redF()) * progress,
        inactive.greenF() + (d->color.greenF() - inactive.greenF()) * progress,
        inactive.blueF() + (d->color.blueF() - inactive.blueF()) * progress);
    QPalette pal = ui->desktopOverviewButton->palette();
    pal.setColor(QPalette::Button, colour);
    ui->desktopOverviewButton->setPalette(pal);

    QFont font = ui->desktopOverviewButton->font();
    font.setBold(progress > 0.5);
    ui->desktopOverviewButton->setFont(font);
}

void TaskbarDesktopWidget::finishAnimation() {
    ui->desktopOverviewButton->setFixedWidth(this->height());
    if (d->progressAnim->currentValue().toReal() > 0.5) {
        ui->applicationsWidget->setFixedWidth(QWIDGETSIZE_MAX);
        updateHighlight(1);
    } else {
        ui->applicationsWidget->setFixedWidth(0);
        updateHighlight(0);
    }
}

void TaskbarDesktopWidget::on_desktopOverviewButton_clicked() {
    DesktopWm::setCurrentDesktop(d->desktop);
}

void TaskbarDesktopWidget::resizeEvent(QResizeEvent* event) {
    if (d->progressAnim->state() == tVariantAnimation::Running) {
        updateAnimation();
    } else {
        finishAnimation();
    }
}
