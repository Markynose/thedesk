#include "standardjobprogresswidget.h"
#include "ui_standardjobprogresswidget.h"

#include "tstandardjob.h"

struct StandardJobProgressWidgetPrivate {
        tStandardJob* job;
};

StandardJobProgressWidget::StandardJobProgressWidget(tStandardJob* job, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::StandardJobProgressWidget) {
    ui->setupUi(this);
    d = new StandardJobProgressWidgetPrivate();
    d->job = job;

    connect(job, &tStandardJob::titleStringChanged, this, &StandardJobProgressWidget::updateState);
    connect(job, &tStandardJob::statusStringChanged, this, &StandardJobProgressWidget::updateState);
    connect(job, &tStandardJob::progressChanged, this, &StandardJobProgressWidget::updateState);
    connect(job, &tStandardJob::totalProgressChanged, this, &StandardJobProgressWidget::updateState);
    connect(job, &tStandardJob::stateChanged, this, &StandardJobProgressWidget::updateState);
    updateState();
}

StandardJobProgressWidget::~StandardJobProgressWidget() {
    delete ui;
    delete d;
}

void StandardJobProgressWidget::updateState() {
    ui->titleLabel->setText(d->job->titleString());
    ui->statusStringLabel->setText(d->job->statusString());
    if (d->job->state() == tJob::Processing) {
        ui->progressBar->setVisible(true);
        ui->progressBar->setMaximum(d->job->totalProgress());
        ui->progressBar->setValue(d->job->progress());
    } else {
        ui->progressBar->setVisible(false);
    }
}
