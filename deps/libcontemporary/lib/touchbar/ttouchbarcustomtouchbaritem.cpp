#include "ttouchbarcustomtouchbaritem.h"
#include "private/ttouchbarcustomtouchbaritem_p.h"

tTouchBarCustomTouchBarItem::tTouchBarCustomTouchBarItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarAbstractItem{identifier, customizationLabel, parent} {
    d = new tTouchBarCustomTouchBarItemPrivate();
}

tTouchBarCustomTouchBarItem::~tTouchBarCustomTouchBarItem() {
    delete d;
}

#ifndef HAVE_TOUCHBAR_IMPL
NSTouchBarItem* tTouchBarCustomTouchBarItem::makeItem() {
    return nullptr;
}
#endif
