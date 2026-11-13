#ifndef TQUEUEGUARDHELPER_H
#define TQUEUEGUARDHELPER_H

#include <QObject>

class tQueueGuardHelper : public QObject {
        Q_OBJECT
    public:
        explicit tQueueGuardHelper(QObject* parent = nullptr);

    signals:
        void unlock();
};

#endif // TQUEUEGUARDHELPER_H
