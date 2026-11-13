#include "tstandardjob.h"

#include "jobs/standardjobprogresswidget.h"

struct tStandardJobPrivate {
        quint64 progress = 0, totalProgress = 0;
        tJob::State state = tJob::Processing;
        QString titleString;
        QString statusString;
        bool transient;
};

tStandardJob::tStandardJob(bool transient, QObject* parent) :
    tJob{parent} {
    d = new tStandardJobPrivate();
    d->transient = transient;
}

tStandardJob::~tStandardJob() {
    delete d;
}

void tStandardJob::setProgress(quint64 progress) {
    d->progress = progress;
    emit progressChanged(progress);
}

void tStandardJob::setTotalProgress(quint64 totalProgress) {
    d->totalProgress = totalProgress;
    emit totalProgressChanged(totalProgress);
}

void tStandardJob::setState(State state) {
    d->state = state;
    emit stateChanged(state);
}

void tStandardJob::setTitleString(QString titleString) {
    d->titleString = titleString;
    emit titleStringChanged(titleString);
}

void tStandardJob::setStatusString(QString statusString) {
    d->statusString = statusString;
    emit statusStringChanged(statusString);
}

QString tStandardJob::titleString() {
    return d->titleString;
}

QString tStandardJob::statusString() {
    return d->statusString;
}

quint64 tStandardJob::progress() {
    return d->progress;
}

quint64 tStandardJob::totalProgress() {
    return d->totalProgress;
}

tJob::State tStandardJob::state() {
    return d->state;
}

QWidget* tStandardJob::makeProgressWidget() {
    return new StandardJobProgressWidget(this);
}

bool tStandardJob::isTransient() {
    return d->transient;
}
