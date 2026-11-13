#include "pluginmanagementpopover.h"
#include "ui_pluginmanagementpopover.h"

#include "knownpluginmodel.h"
#include "tpluginmanagerhelper.h"
#include <QUuid>
#include <tapplication.h>

struct PluginManagementPopoverPrivate {
        KnownPluginModel* model;
        QUuid modifyingPlugin;
};

PluginManagementPopover::PluginManagementPopover(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::PluginManagementPopover) {
    ui->setupUi(this);
    d = new PluginManagementPopoverPrivate();
    d->model = new KnownPluginModel(this);
    ui->listView->setModel(d->model);
    ui->stackedWidget->setCurrentAnimation(tStackedWidget::SlideHorizontal);

    ui->restartPrompt->setText(tr("Some changes to plugins won't apply until you restart %1.").arg(tApplication::applicationDisplayName()));
    ui->restartButton->setText(tr("Restart %1").arg(tApplication::applicationDisplayName()));
    ui->restartButton->setProperty("type", "destructive");

    connect(tPluginManagerHelper::instance(), &tPluginManagerHelper::blacklistChanged, this, &PluginManagementPopover::updateCurrentPlugin);
}

PluginManagementPopover::~PluginManagementPopover() {
    delete d;
    delete ui;
}

void PluginManagementPopover::on_titleLabel_backButtonClicked() {
    emit done();
}

void PluginManagementPopover::on_listView_activated(const QModelIndex& index) {
    d->modifyingPlugin = index.data(KnownPluginModel::UuidRole).toUuid();
    ui->titleLabel_2->setText(index.data().toString());
    updateCurrentPlugin();
    ui->stackedWidget->setCurrentWidget(ui->pluginDetailPage);
}

void PluginManagementPopover::on_titleLabel_2_backButtonClicked() {
    ui->stackedWidget->setCurrentWidget(ui->pluginListPage);
}

void PluginManagementPopover::updateCurrentPlugin() {
    ui->deactivateButton->setVisible(false);
    ui->activateButton->setVisible(false);
    ui->restartButton->setVisible(false);
    ui->restartPrompt->setVisible(false);
    if (tPluginManagerHelper::instance()->blacklistedPlugins().contains(d->modifyingPlugin)) {
        ui->activateButton->setVisible(true);
    } else {
        ui->deactivateButton->setVisible(true);
    }

    if (tPluginManagerHelper::instance()->havePendingPluginChanges()) {
        ui->restartPrompt->setVisible(true);
        ui->restartButton->setVisible(true);
    }
}

void PluginManagementPopover::on_activateButton_clicked() {
    tPluginManagerHelper::instance()->unblacklist(d->modifyingPlugin);
    if (!tPluginManagerHelper::instance()->loadedPlugins().contains(d->modifyingPlugin)) {
        tPluginManagerHelper::instance()->loadPlugin(d->modifyingPlugin);
    }
}

void PluginManagementPopover::on_deactivateButton_clicked() {
    tPluginManagerHelper::instance()->blacklist(d->modifyingPlugin);
}

void PluginManagementPopover::on_restartButton_clicked() {
    tApplication::restart();
}
