#include "ttouchbar.h"
#include "private/ttouchbar_p.h"

#include "ttouchbarabstractitem.h"

tTouchBar::tTouchBar(QObject* parent) :
    QObject{parent} {
    d = new tTouchBarPrivate();

    this->init();
}

tTouchBar::~tTouchBar() {
    delete d;
}

void tTouchBar::addItem(tTouchBarAbstractItem* item) {
    if (!d->touchBarItems.contains(item)) {
        d->touchBarItems.append(item);
        connect(item, &tTouchBarAbstractItem::invalidateTouchBar, this, &tTouchBar::invalidateTouchBar);
    }
    this->invalidateTouchBar();
}

QList<tTouchBarAbstractItem*> tTouchBar::touchBarItems() {
    return d->touchBarItems;
}

void tTouchBar::addDefaultItem(tTouchBarAbstractItem* item) {
    this->addItem(item);
    d->defaultTouchBarItems.append(item);
    this->invalidateTouchBar();
}

QList<tTouchBarAbstractItem*> tTouchBar::defaultItems() {
    return d->defaultTouchBarItems;
}

QString tTouchBar::customizationIdentifier() {
    return d->customizationIdentifier;
}

void tTouchBar::setCustomizationIdentifier(QString customizationIdentifier) {
    d->customizationIdentifier = customizationIdentifier;
    this->invalidateTouchBar();
}

#ifndef HAVE_TOUCHBAR_IMPL
void tTouchBar::attach(QWidget* widget) {
    Q_UNUSED(widget)
}

void tTouchBar::detach() {
}

void tTouchBar::init() {
}

void tTouchBar::invalidateTouchBar() {
}

NSTouchBar* tTouchBar::makeTouchBar() {
    return nullptr;
}

void tTouchBar::addCustomizationMenuItem(QMenu* menu) {
}
#endif
