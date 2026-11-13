#ifndef TQUEUEGUARD_H
#define TQUEUEGUARD_H

#include "libcontemporary_global.h"
#include <QCoroTask>
#include <QObject>

struct tQueueGuardPrivate;
class LIBCONTEMPORARY_EXPORT tQueueGuard : public QObject {
        Q_OBJECT
    public:
        explicit tQueueGuard(QObject* parent = nullptr);
        ~tQueueGuard();

        struct GuardLockPrivate;
        class LIBCONTEMPORARY_EXPORT GuardLock {
            public:
                GuardLock(GuardLock&& other);
                ~GuardLock();

            protected:
                friend tQueueGuard;
                GuardLock(tQueueGuard* lock);

            private:
                GuardLockPrivate* d;
        };

        QCoro::Task<GuardLock> guardQueue();

    signals:

    protected:
        void unlock();

    private:
        tQueueGuardPrivate* d;
};

#endif // TQUEUEGUARD_H
