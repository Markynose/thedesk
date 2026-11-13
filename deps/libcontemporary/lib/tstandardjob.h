#ifndef TSTANDARDJOB_H
#define TSTANDARDJOB_H

#include "libcontemporary_global.h"
#include "tjob.h"

struct tStandardJobPrivate;
class LIBCONTEMPORARY_EXPORT tStandardJob : public tJob {
        Q_OBJECT
    public:
        explicit tStandardJob(bool transient = false, QObject* parent = nullptr);
        ~tStandardJob();

        void setProgress(quint64 progress);
        void setTotalProgress(quint64 totalProgress);
        void setState(State state);
        void setTitleString(QString titleString);
        void setStatusString(QString statusString);

        QString titleString();
        QString statusString();

    private:
        tStandardJobPrivate* d;

        // tJob interface
    public:
        quint64 progress();
        quint64 totalProgress();
        State state();
        QWidget* makeProgressWidget();
        bool isTransient();
};

#endif // TSTANDARDJOB_H
