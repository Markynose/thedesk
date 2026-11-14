#include "jobwidget.h"
#include "ui_jobwidget.h"

#include "pebble.h"
#include <QPainter>

struct JobWidgetPrivate {
        bool loadingData = true;
        SystemJobPtr job;
};

JobWidget::JobWidget(SystemJobPtr job, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::JobWidget) {
    ui->setupUi(this);

    d = new JobWidgetPrivate();
    d->job = job;

    // A card in its app's deck: title (and how far, at the end), status
    const M3::Scheme s = M3::currentScheme();
    ui->verticalLayout->setContentsMargins(16, 12, 16, 12);
    ui->verticalLayout->setSpacing(2);
    ui->summaryLabel->setFont(Pebble::flex(15, 600));
    ui->summaryLabel->setContentsMargins(0, 0, 52, 0);
    ui->bodyLabel->setFont(Pebble::flex(14, 400));
    for (QLabel* label : {ui->summaryLabel, ui->bodyLabel}) {
        QPalette pal = label->palette();
        for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, label == ui->summaryLabel ? s.onSurface : s.onSurfaceVariant);
        label->setPalette(pal);
    }
    ui->progressBar->hide(); // it keeps the numbers; the ring shows them
    connect(ui->progressBar, &QProgressBar::valueChanged, this, qOverload<>(&QWidget::update));

    connect(job.data(), &SystemJob::titleChanged, ui->summaryLabel, &QLabel::setText);
    connect(job.data(), &SystemJob::statusChanged, ui->bodyLabel, &QLabel::setText);
    connect(job.data(), &SystemJob::totalProgressChanged, this, [this](quint64 totalProgress) {
        ui->progressBar->setMaximum(totalProgress);
    });
    connect(job.data(), &SystemJob::progressChanged, ui->progressBar, &QProgressBar::setValue);

    connect(job.data(), &SystemJob::stateChanged, this, &JobWidget::updateState);
    connect(job.data(), &SystemJob::validChanged, this, [this](bool valid) {
        if (!valid && !d->loadingData) this->deleteLater();
    });

    loadData();
}

JobWidget::~JobWidget() {
    delete ui;
    delete d;
}

double JobWidget::progress() const {
    const int max = ui->progressBar->maximum() - ui->progressBar->minimum();
    return max <= 0 ? 0 : qBound(0.0, double(ui->progressBar->value() - ui->progressBar->minimum()) / max, 1.0);
}

void JobWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath card;
    card.addRoundedRect(QRectF(this->rect()), 20, 20);
    p.fillPath(card, s.surfaceContainerHigh);
    p.setFont(Pebble::flex(13, 600));
    p.setPen(s.primary);
    p.drawText(QRectF(this->width() - 16 - 60, 12, 60, 22), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1%").arg(qRound(progress() * 100)));
}

QCoro::Task<> JobWidget::loadData() {
    d->loadingData = true;
    ui->summaryLabel->setText(co_await d->job->title());
    ui->bodyLabel->setText(co_await d->job->status());
    ui->progressBar->setMaximum(co_await d->job->totalProgress());
    ui->progressBar->setValue(co_await d->job->progress());

    updateState(co_await d->job->state());
    if (!d->job->valid()) this->deleteLater();
    d->loadingData = false;
}

void JobWidget::updateState(QString state) {
    if (state == QStringLiteral("Processing")) {
    } else if (state == QStringLiteral("Finished")) {
        this->deleteLater();
    } else if (state == QStringLiteral("Failed")) {
        this->deleteLater();
    } else if (state == QStringLiteral("RequiresAttention")) {
    }
}
