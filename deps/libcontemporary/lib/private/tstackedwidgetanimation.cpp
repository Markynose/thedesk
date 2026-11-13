#include "tstackedwidgetanimation.h"

#include "tstackedwidget.h"

struct tStackedWidgetAnimationPrivate {
        QWidget* oldWidget;
        QWidget* newWidget;
        tStackedWidget* parentStack;
        bool movingForwards;
        int newIndex;
};

tStackedWidgetAnimation::tStackedWidgetAnimation(int oldIndex, int newIndex, tStackedWidget* parent) :
    QObject{parent} {
    d = new tStackedWidgetAnimationPrivate();
    d->oldWidget = parent->widget(oldIndex);
    d->newWidget = parent->widget(newIndex);
    d->parentStack = parent;
    d->movingForwards = oldIndex < newIndex;
    d->newIndex = newIndex;
}

tStackedWidgetAnimation::~tStackedWidgetAnimation() {
    delete d;
}

QWidget* tStackedWidgetAnimation::oldWidget() {
    return d->oldWidget;
}

QWidget* tStackedWidgetAnimation::newWidget() {
    return d->newWidget;
}

tStackedWidget* tStackedWidgetAnimation::parentStack() {
    return d->parentStack;
}

bool tStackedWidgetAnimation::movingForwards() {
    return d->movingForwards;
}

void tStackedWidgetAnimation::emitDone() {
    emit done(d->newIndex);
}
