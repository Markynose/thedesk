#include "powersettings.h"
#include "ui_powersettings.h"

#include "batteryhealth.h"

#include <DesktopPowerProfiles/desktoppowerprofiles.h>
#include <QIcon>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tsettings.h>

struct PowerSettingsPrivate {
        tSettings settings;
        DesktopPowerProfiles* profiles;

        const static QStringList timeoutUnits;
        const static QStringList powerActions;

        // Timeouts are presets plus Custom (the exact number and unit, in a row of its own)
        struct Preset {
                int value;
                QString unit;
        };
        const QList<Preset> presets = {{1, "min"}, {2, "min"}, {5, "min"}, {10, "min"}, {15, "min"}, {30, "min"}, {1, "hr"}, {0, "never"}};
        bool screenOffCustom = false, suspendCustom = false;
};

const QStringList PowerSettingsPrivate::timeoutUnits = {
    "sec",
    "min",
    "hr",
    "never"};

const QStringList PowerSettingsPrivate::powerActions = {
    "ask",
    "poweroff",
    "reboot",
    "suspend",
    "hibernate",
    "ignore"};

PowerSettings::PowerSettings(DesktopPowerProfiles* powerProfiles) :
    StatusCenterPane(),
    ui(new Ui::PowerSettings) {
    ui->setupUi(this);

    d = new PowerSettingsPrivate();
    d->profiles = powerProfiles;

    ui->titleLabel->setBackButtonIsMenu(true);
    ui->titleLabel->setBackButtonShown(StateManager::instance()->statusCenterManager()->isHamburgerMenuRequired());
    connect(StateManager::instance()->statusCenterManager(), &StatusCenterManager::isHamburgerMenuRequiredChanged, ui->titleLabel, &tTitleLabel::setBackButtonShown);

    for (QComboBox* box : {ui->screenOffPreset, ui->suspendPreset}) {
        for (const PowerSettingsPrivate::Preset& preset : d->presets) {
            if (preset.unit == "never") box->addItem(tr("Never"));
            else if (preset.unit == "hr") box->addItem(preset.value == 1 ? tr("1 hour") : tr("%1 hours").arg(preset.value));
            else box->addItem(preset.value == 1 ? tr("1 minute") : tr("%1 minutes").arg(preset.value));
        }
        box->addItem(tr("Custom…"));
    }
    connect(ui->screenOffPreset, &QComboBox::activated, this, [this](int index) {
        pickPreset(index, "screenoff", &d->screenOffCustom);
    });
    connect(ui->suspendPreset, &QComboBox::activated, this, [this](int index) {
        pickPreset(index, "suspend", &d->suspendCustom);
    });
    for (QPushButton* button : {ui->powerStretchProfileButton, ui->balancedProfileButton, ui->performanceProfileButton}) button->setAutoExclusive(true);
    ui->powerStretchProfileButton->setIcon(QIcon::fromTheme("eco-symbolic"));
    ui->balancedProfileButton->setIcon(QIcon::fromTheme("balance-symbolic"));
    ui->performanceProfileButton->setIcon(QIcon::fromTheme("speed-symbolic"));

    const int contentWidth = StateManager::instance()->statusCenterManager()->preferredContentWidth();
    ui->powerProfileWidget->setFixedWidth(contentWidth);
    ui->timeoutsWidget->setFixedWidth(contentWidth);
    ui->buttonsWidget->setFixedWidth(contentWidth);

    auto* health = new BatteryHealth(this);
    health->setFixedWidth(contentWidth);
    ui->verticalLayout_2->insertWidget(0, health, 0, Qt::AlignHCenter);

    connect(&d->settings, &tSettings::settingChanged, this, &PowerSettings::settingChanged);
    for (QString setting : {
             "Power/timeouts.screenoff.value",
             "Power/timeouts.screenoff.unit",
             "Power/timeouts.suspend.value",
             "Power/timeouts.suspend.unit",
             "Power/actions.powerbutton",
             "Power/suspend.lockScreen"}) {
        settingChanged(setting, d->settings.value(setting));
    }
    d->screenOffCustom = !matchPreset("screenoff");
    d->suspendCustom = !matchPreset("suspend");
    showPresets();

    connect(d->profiles, &DesktopPowerProfiles::powerProfileChanged, this, &PowerSettings::updatePowerProfiles);
    updatePowerProfiles();
}

PowerSettings::~PowerSettings() {
    delete d;
    delete ui;
}

QString PowerSettings::name() {
    return "PowerSettings";
}

QString PowerSettings::displayName() {
    return tr("Power");
}

QIcon PowerSettings::icon() {
    return QIcon::fromTheme("preferences-system-power-symbolic");
}

QWidget* PowerSettings::leftPane() {
    return nullptr;
}

void PowerSettings::on_titleLabel_backButtonClicked() {
    StateManager::instance()->statusCenterManager()->showStatusCenterHamburgerMenu();
}

void PowerSettings::on_screenOffSpin_valueChanged(int arg1) {
    d->settings.setValue("Power/timeouts.screenoff.value", arg1);
}

void PowerSettings::on_screenOffUnit_currentIndexChanged(int index) {
    d->settings.setValue("Power/timeouts.screenoff.unit", d->timeoutUnits.value(index));
}

void PowerSettings::settingChanged(QString key, QVariant value) {
    if (key == "Power/timeouts.screenoff.value") {
        ui->screenOffSpin->setValue(value.toInt());
    } else if (key == "Power/timeouts.screenoff.unit") {
        QString unit = value.toString();
        ui->screenOffUnit->setCurrentIndex(d->timeoutUnits.indexOf(unit));
        ui->screenOffSpin->setEnabled(unit != "never");
    } else if (key == "Power/timeouts.suspend.value") {
        ui->suspendSpin->setValue(value.toInt());
    } else if (key == "Power/timeouts.suspend.unit") {
        QString unit = value.toString();
        ui->suspendUnit->setCurrentIndex(d->timeoutUnits.indexOf(unit));
        ui->suspendSpin->setEnabled(unit != "never");
    }
    if (key.startsWith("Power/timeouts.")) showPresets();
    if (key == "Power/actions.powerbutton") {
        ui->powerButtonActionBox->setCurrentIndex(d->powerActions.indexOf(value.toString()));
    } else if (key == "Power/suspend.lockScreen") {
        ui->lockScreenAfterSuspendSwitch->setChecked(value.toBool());
    }
}

int PowerSettings::matchPreset(const QString& which) {
    const QString unit = d->settings.value("Power/timeouts." + which + ".unit").toString();
    const int value = d->settings.value("Power/timeouts." + which + ".value").toInt();
    for (int i = 0; i < d->presets.size(); i++) {
        if (d->presets.at(i).unit == unit && (unit == "never" || d->presets.at(i).value == value)) return i + 1;
    }
    return 0;
}

void PowerSettings::showPresets() {
    // Index 0 means none matches: Custom, the last item
    const int screenOff = matchPreset("screenoff"), suspend = matchPreset("suspend");
    QSignalBlocker blocker1(ui->screenOffPreset);
    QSignalBlocker blocker2(ui->suspendPreset);
    ui->screenOffPreset->setCurrentIndex(d->screenOffCustom || !screenOff ? d->presets.size() : screenOff - 1);
    ui->suspendPreset->setCurrentIndex(d->suspendCustom || !suspend ? d->presets.size() : suspend - 1);
    for (QWidget* w : {static_cast<QWidget*>(ui->screenOffCustomLabel), static_cast<QWidget*>(ui->screenOffSpin), static_cast<QWidget*>(ui->screenOffUnit)}) w->setVisible(d->screenOffCustom || !screenOff);
    for (QWidget* w : {static_cast<QWidget*>(ui->suspendCustomLabel), static_cast<QWidget*>(ui->suspendSpin), static_cast<QWidget*>(ui->suspendUnit)}) w->setVisible(d->suspendCustom || !suspend);
}

void PowerSettings::pickPreset(int index, const QString& which, bool* custom) {
    *custom = index >= d->presets.size();
    if (!*custom) {
        d->settings.setValue("Power/timeouts." + which + ".value", qMax(1, d->presets.at(index).value));
        d->settings.setValue("Power/timeouts." + which + ".unit", d->presets.at(index).unit);
    }
    showPresets();
}

void PowerSettings::updatePowerProfiles() {
    bool powerProfilesAvailable = d->profiles->powerProfilesAvailable();
    ui->powerProfileWidget->setVisible(powerProfilesAvailable);

    bool isPerformanceAvailable = d->profiles->isPerformanceAvailable();
    ui->performanceProfileButton->setVisible(isPerformanceAvailable);

    switch (d->profiles->currentPowerProfile()) {
        case DesktopPowerProfiles::PowerStretch:
            ui->powerStretchProfileButton->setChecked(true);
            break;
        case DesktopPowerProfiles::Balanced:
            ui->balancedProfileButton->setChecked(true);
            break;
        case DesktopPowerProfiles::Performance:
            ui->performanceProfileButton->setChecked(true);
            break;
        case DesktopPowerProfiles::Unknown:
            break;
    }
    // Only the chosen profile's line shows under the group
    ui->label_10->setVisible(ui->powerStretchProfileButton->isChecked());
    ui->label_9->setVisible(ui->balancedProfileButton->isChecked());
    ui->performanceDescription->setVisible(ui->performanceProfileButton->isChecked());

    if (!d->profiles->performanceInhibited().isEmpty()) {
        ui->performanceInhibition->setTitle(tr("Performance mode unavailable"));
        ui->performanceInhibition->setText(performanceInhibitionReason(d->profiles->performanceInhibited()).arg(tr("unavailable")));
        ui->performanceInhibition->setState(tStatusFrame::Error);
        ui->performanceProfileButton->setEnabled(false);
        ui->performanceInhibition->setVisible(true);
    } else if (!d->profiles->performanceDegraded().isEmpty()) {
        ui->performanceInhibition->setTitle(tr("Performance mode temporarily unavailable"));
        ui->performanceInhibition->setText(performanceInhibitionReason(d->profiles->performanceDegraded()).arg(tr("temporarily unavailable")));
        ui->performanceInhibition->setState(tStatusFrame::Warning);
        ui->performanceProfileButton->setEnabled(true);
        ui->performanceInhibition->setVisible(true);
    } else {
        ui->performanceInhibition->setVisible(false);
        ui->performanceProfileButton->setEnabled(true);
    }
}

QString PowerSettings::performanceInhibitionReason(QString reason) {
    if (reason == "lap-detected") return tr("Performance mode is %1 because the device is on your lap.");
    if (reason == "high-operating-temperature") return tr("Performance mode is %1 because your device is getting warm.");
    return tr("Performance mode is %1.");
}

void PowerSettings::on_suspendSpin_valueChanged(int arg1) {
    d->settings.setValue("Power/timeouts.suspend.value", arg1);
}

void PowerSettings::on_suspendUnit_currentIndexChanged(int index) {
    d->settings.setValue("Power/timeouts.suspend.unit", d->timeoutUnits.value(index));
}

void PowerSettings::on_powerButtonActionBox_currentIndexChanged(int index) {
    d->settings.setValue("Power/actions.powerbutton", d->powerActions.value(index));
}

void PowerSettings::on_balancedProfileButton_toggled(bool checked) {
    if (checked) d->profiles->setCurrentPowerProfile(DesktopPowerProfiles::Balanced);
}

void PowerSettings::on_powerStretchProfileButton_toggled(bool checked) {
    if (checked) d->profiles->setCurrentPowerProfile(DesktopPowerProfiles::PowerStretch);
}

void PowerSettings::on_performanceProfileButton_toggled(bool checked) {
    if (checked) d->profiles->setCurrentPowerProfile(DesktopPowerProfiles::Performance);
}

void PowerSettings::on_lockScreenAfterSuspendSwitch_toggled(bool checked) {
    d->settings.setValue("Power/suspend.lockScreen", checked);
}

void PowerSettings::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        emit displayNameChanged();
    }
}
