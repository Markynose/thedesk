#include "ttouchbargroupitem.h"
#include "private/ttouchbargroupitem_p.h"

tTouchBarGroupItem::tTouchBarGroupItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarAbstractItem{identifier, customizationLabel, parent} {
    d = new tTouchBarGroupItemPrivate();
}

tTouchBarGroupItem::~tTouchBarGroupItem() {
    delete d;
}

void tTouchBarGroupItem::setGroupTouchBar(tTouchBar* touchBar) {
    d->groupTouchBar = touchBar;
    this->updateItem();
}

tTouchBar* tTouchBarGroupItem::groupTouchBar() {
    return d->groupTouchBar;
}

tTouchBarAlertStyleGroupItem::tTouchBarAlertStyleGroupItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarGroupItem{identifier, customizationLabel, parent} {
}

tTouchBarAlertStyleGroupItem::~tTouchBarAlertStyleGroupItem() {
}

#ifndef HAVE_TOUCHBAR_IMPL
NSTouchBarItem* tTouchBarGroupItem::makeItem() {
    return nullptr;
}

void tTouchBarGroupItem::updateItem() {
}

void tTouchBarAlertStyleGroupItem::updateItem() {
}
#endif
