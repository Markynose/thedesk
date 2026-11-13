#ifndef SLEEPMONITOR_H
#define SLEEPMONITOR_H

#include <QObject>

struct SleepMonitorPrivate;

class SleepMonitor : public QObject {
        Q_OBJECT
    public:
        explicit SleepMonitor(QObject* parent = nullptr);
        ~SleepMonitor();

    private slots:
        void PrepareForSleep(bool sleeping);

    private:
        void acquireInhibitor();
        void releaseInhibitor();

    signals:

    private:
        SleepMonitorPrivate* d;
};

#endif // SLEEPMONITOR_H
