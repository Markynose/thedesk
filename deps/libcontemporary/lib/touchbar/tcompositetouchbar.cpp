#include "tcompositetouchbar.h"

#include <QPointer>

struct tCompositeTouchBarPrivate {
        QWidget* attachedWidget = nullptr;
        QPointer<tTouchBar> currentTouchBar;
};

tCompositeTouchBar::tCompositeTouchBar(QObject* parent) :
    tTouchBar{parent} {
    d = new tCompositeTouchBarPrivate();
}

tCompositeTouchBar::~tCompositeTouchBar() {
    delete d;
}

void tCompositeTouchBar::setCurrentTouchBar(tTouchBar* touchBar) {
    if (d->currentTouchBar) {
        d->currentTouchBar->disconnect(this);
    }

    auto oldAttachedWidget = d->attachedWidget;
    this->detach();
    d->currentTouchBar = touchBar;
    this->attach(oldAttachedWidget);
    if (d->currentTouchBar) {
        connect(d->currentTouchBar, &tTouchBar::destroyed, this, [this] {
            this->setCurrentTouchBar(nullptr);
        });
    }
}

tTouchBar* tCompositeTouchBar::currentTouchBar() {
    return d->currentTouchBar;
}

void tCompositeTouchBar::attach(QWidget* widget) {
    if (!widget) return;

    d->attachedWidget = widget;
    if (d->currentTouchBar) {
        d->currentTouchBar->attach(widget);
    }
}

void tCompositeTouchBar::detach() {
    if (d->currentTouchBar) {
        d->currentTouchBar->detach();
    }
    d->attachedWidget = nullptr;
}
