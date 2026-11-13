#include "tpluginmanagerpane.h"
#include "ui_tpluginmanagerpane.h"

#include "pluginmanagementpopover.h"
#include "tapplication.h"
#include "tpluginmanagerhelper.h"
#include "tpopover.h"

tPluginManagerPane::tPluginManagerPane(QWidget* parent) :
    tSettingsPane(parent),
    ui(new Ui::tPluginManagerPane) {
    ui->setupUi(this);

    ui->descriptionLabel->setText(tr("Plugins add core and extra functionality to %1.").arg(tApplication::applicationDisplayName()));
    ui->restartLabel->setText(tr("Restart %1 to apply changes in plugins").arg(tApplication::applicationDisplayName()));
    ui->restartButton->setProperty("type", "destructive");

    connect(tPluginManagerHelper::instance(), &tPluginManagerHelper::blacklistChanged, this, &tPluginManagerPane::updateRestartState);
    updateRestartState();
}

tPluginManagerPane::~tPluginManagerPane() {
    delete ui;
}

QString tPluginManagerPane::paneName() {
    return tr("Plugins");
}

void tPluginManagerPane::updateRestartState() {
    ui->restartWidget->setVisible(tPluginManagerHelper::instance()->havePendingPluginChanges());
}

void tPluginManagerPane::on_manageButton_clicked() {
    auto* jp = new PluginManagementPopover();
    auto* popover = new tPopover(jp);
    popover->setPopoverWidth(600);
    popover->setPopoverSide(tPopover::Trailing);
    connect(jp, &PluginManagementPopover::done, popover, &tPopover::dismiss);
    connect(popover, &tPopover::dismissed, popover, &tPopover::deleteLater);
    connect(popover, &tPopover::dismissed, jp, &PluginManagementPopover::deleteLater);
    popover->show(this->window());
}

void tPluginManagerPane::on_restartButton_clicked() {
    tApplication::restart();
}
