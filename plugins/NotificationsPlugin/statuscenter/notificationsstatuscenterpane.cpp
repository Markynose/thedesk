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
#include "notificationsstatuscenterpane.h"
#include "ui_notificationsstatuscenterpane.h"

#include "notification.h"
#include "notificationappgroup.h"
#include "notificationtracker.h"
#include "pebble.h"

#include <quietmodemanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>

#include <SystemJob/systemjobcontroller.h>

#include <QHBoxLayout>
#include <QPushButton>
#include <m3motion.h>

struct NotificationsStatusCenterPanePrivate {
        NotificationTracker* tracker;
        SystemJobController* jobController;
        QMap<QString, NotificationAppGroup*> groups;
        QMap<QuietModeManagerTd::QuietMode, QPushButton*> quietButtons;
        QList<M3SpringAnimation*> quietGrow;
        QHBoxLayout* quietLayout;
};

NotificationsStatusCenterPane::NotificationsStatusCenterPane(NotificationTracker* tracker, SystemJobController* jobController) :
    StatusCenterPane(),
    ui(new Ui::NotificationsStatusCenterPane) {
    ui->setupUi(this);

    d = new NotificationsStatusCenterPanePrivate();
    d->tracker = tracker;
    d->jobController = jobController;

    ui->titleLabel->setBackButtonIsMenu(true);
    ui->titleLabel->setBackButtonShown(StateManager::instance()->statusCenterManager()->isHamburgerMenuRequired());
    connect(StateManager::instance()->statusCenterManager(), &StatusCenterManager::isHamburgerMenuRequiredChanged, ui->titleLabel, &tTitleLabel::setBackButtonShown);

    const int contentWidth = StateManager::instance()->statusCenterManager()->preferredContentWidth();
    ui->notificationsWidget->setFixedWidth(contentWidth);
    ui->quietModeWidget->setFixedWidth(contentWidth);

    connect(d->tracker, &NotificationTracker::newNotification, this, [=](NotificationPtr notification) {
        auto application = notification->application();
        // without a desktop entry, apps are told apart by the name they gave
        auto group = appGroupForDesktopEntry(Pebble::key(application), application);
        group->pushNotification(notification);

        ui->stackedWidget->setCurrentWidget(ui->notificationsPage);
    });

    connect(d->jobController, &SystemJobController::newJob, this, &NotificationsStatusCenterPane::registerJob);
    for (const auto& job : d->jobController->jobs()) {
        registerJob(job);
    }

    ui->stackedWidget->setCurrentAnimation(tStackedWidget::Fade);
    ui->notificationSplash->setPixmap(QIcon::fromTheme("notifications").pixmap(QSize(128, 128)));

    ui->titleLayout->setStretch(0, 1);
    ui->clearAllButton->setProperty("m3ButtonSize", QStringLiteral("xs"));
    ui->clearAllButton->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    ui->clearAllButton->hide();
    connect(ui->clearAllButton, &QPushButton::clicked, this, [this] {
        for (NotificationAppGroup* group : d->groups) group->dismissAll();
    });

    // Quiet modes as a connected button group: the selected one widens to
    // show its name
    QuietModeManagerTd* quiet = StateManager::quietModeManager();
    auto* quietGroup = new QWidget(this);
    quietGroup->setProperty("m3ButtonGroup", QStringLiteral("connected"));
    d->quietLayout = new QHBoxLayout(quietGroup);
    d->quietLayout->setContentsMargins(0, 0, 0, 0);
    d->quietLayout->setSpacing(2);
    ui->quietModesLayout->addWidget(quietGroup);
    ui->horizontalSpacer->changeSize(0, 0);
    for (QuietModeManagerTd::QuietMode mode : quiet->availableQuietModes()) {
        QPushButton* button = new QPushButton(QIcon::fromTheme(QuietModeManagerTd::icon(mode)), QString(), quietGroup);
        button->setToolTip(QStringLiteral("%1: %2").arg(QuietModeManagerTd::name(mode), QuietModeManagerTd::description(mode)));
        button->setAccessibleName(QuietModeManagerTd::name(mode));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        connect(button, &QPushButton::clicked, quiet, [quiet, mode] {
            quiet->setQuietMode(mode);
        });
        d->quietLayout->addWidget(button);
        d->quietButtons.insert(mode, button);

        auto* grow = new M3SpringAnimation(this);
        grow->setToken(M3Motion::SpatialFast);
        grow->snapTo(0.0);
        const int index = d->quietGrow.count();
        connect(grow, &M3SpringAnimation::valueChanged, this, [this, index](QVariant value) {
            d->quietLayout->setStretch(index, 100 + qRound(150 * value.toDouble()));
        });
        d->quietGrow.append(grow);
    }
    auto updateQuietMode = [this] {
        const QuietModeManagerTd::QuietMode mode = StateManager::quietModeManager()->currentMode();
        int i = 0;
        for (auto button = d->quietButtons.cbegin(); button != d->quietButtons.cend(); ++button, ++i) {
            const bool selected = button.key() == mode;
            if (selected) button.value()->setChecked(true);
            button.value()->setText(selected ? QuietModeManagerTd::name(button.key()) : QString());
            d->quietGrow.at(d->quietLayout->indexOf(button.value()))->animateTo(selected ? 1.0 : 0.0);
        }
    };
    connect(quiet, &QuietModeManagerTd::quietModeChanged, this, updateQuietMode);
    updateQuietMode();
    for (M3SpringAnimation* grow : d->quietGrow) grow->snapTo(grow->endValue());
}

NotificationsStatusCenterPane::~NotificationsStatusCenterPane() {
    delete d;
    delete ui;
}

void NotificationsStatusCenterPane::on_titleLabel_backButtonClicked() {
    StateManager::statusCenterManager()->showStatusCenterHamburgerMenu();
}

NotificationAppGroup* NotificationsStatusCenterPane::appGroupForDesktopEntry(QString desktopEntry, ApplicationPointer application) {
    if (d->groups.contains(desktopEntry)) {
        return d->groups.value(desktopEntry);
    } else {
        auto group = new NotificationAppGroup(application, this);
        // Emptied, it spins away; a new notification from the app gets a new group
        connect(group, &NotificationAppGroup::emptied, this, [=] {
            if (d->groups.value(desktopEntry) == group) d->groups.remove(desktopEntry);
            ui->clearAllButton->setVisible(!d->groups.isEmpty());
        });
        connect(
            group, &NotificationAppGroup::destroyed, this, [=] {
                ui->notificationsLayout->removeWidget(group);
                if (d->groups.value(desktopEntry) == group) d->groups.remove(desktopEntry);

                if (d->groups.isEmpty()) ui->stackedWidget->setCurrentWidget(ui->noNotificationsPage);
                ui->clearAllButton->setVisible(!d->groups.isEmpty());
            },
            Qt::QueuedConnection);
        ui->notificationsLayout->insertWidget(0, group);
        d->groups.insert(desktopEntry, group);
        ui->clearAllButton->show();

        return group;
    }
}

void NotificationsStatusCenterPane::registerJob(SystemJobPtr job) {
    auto desktopEntry = d->jobController->desktopEntryForService(job->service());
    // without a desktop entry, it joins the notifications under its app's name
    auto app = desktopEntry.isEmpty() ? ApplicationPointer(new Application(QVariantMap{{"Icon", "generic-app"}, {"Name", d->jobController->applicationNameForService(job->service())}}))
                                      : ApplicationPointer(new Application(desktopEntry));

    auto group = appGroupForDesktopEntry(Pebble::key(app), app);
    group->pushJob(job);

    ui->stackedWidget->setCurrentWidget(ui->notificationsPage);
}

QString NotificationsStatusCenterPane::name() {
    return "NotificationsPane";
}

QString NotificationsStatusCenterPane::displayName() {
    return tr("Notifications");
}

QIcon NotificationsStatusCenterPane::icon() {
    // thedesk-notifications isn't in every icon theme
    QIcon icon = QIcon::fromTheme("thedesk-notifications");
    return icon.isNull() ? QIcon::fromTheme("preferences-desktop-notification-symbolic") : icon;
}

QWidget* NotificationsStatusCenterPane::leftPane() {
    return nullptr;
}

void NotificationsStatusCenterPane::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        emit displayNameChanged();
    }
}
