#include "tqueueguard.h"

#include "private/tqueueguardhelper.h"
#include <QCoroCore>
#include <QMutex>
#include <QQueue>

struct tQueueGuardPrivate {
        QMutex guarded;
        QQueue<tQueueGuardHelper*> queue;
};

struct tQueueGuard::GuardLockPrivate {
        tQueueGuard* lock;
};

tQueueGuard::tQueueGuard(QObject* parent) :
    QObject{parent} {
    d = new tQueueGuardPrivate();
}

tQueueGuard::~tQueueGuard() {
    delete d;
}

QCoro::Task<tQueueGuard::GuardLock> tQueueGuard::guardQueue() {
    while (!d->guarded.tryLock(0)) {
        // Create an object to order operations
        auto o = new tQueueGuardHelper();
        d->queue.enqueue(o);

        // Wait for the object to be destroyed in unlock()
        co_await qCoro(o, &tQueueGuardHelper::unlock);
    }

    co_return GuardLock(this);
}

void tQueueGuard::unlock() {
    // Unlock the mutex
    d->guarded.unlock();

    if (!d->queue.isEmpty()) {
        // Trigger the next item in the queue
        auto helper = d->queue.dequeue();
        emit helper->unlock();
        helper->deleteLater();
    }
}

tQueueGuard::GuardLock::GuardLock(tQueueGuard* lock) {
    d = new GuardLockPrivate();
    d->lock = lock;
}

tQueueGuard::GuardLock::GuardLock(GuardLock&& other) {
    this->d = other.d;
    other.d = nullptr;
}

tQueueGuard::GuardLock::~GuardLock() {
    if (this->d) {
        d->lock->unlock();
        delete d;
    }
}
