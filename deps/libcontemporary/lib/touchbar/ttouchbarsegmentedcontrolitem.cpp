#include "ttouchbarsegmentedcontrolitem.h"
#include "private/ttouchbarsegmentedcontrolitem_p.h"

tTouchBarSegmentedControlItem::tTouchBarSegmentedControlItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarCustomTouchBarItem{identifier, customizationLabel, parent} {
    d = new tTouchBarSegmentedControlItemPrivate();
    this->init();
}

tTouchBarSegmentedControlItem::~tTouchBarSegmentedControlItem() {
    this->tearDown();
    delete d;
}

void tTouchBarSegmentedControlItem::setSegments(QStringList segments) {
    d->segments = segments;
    this->updateItems();
}

QStringList tTouchBarSegmentedControlItem::segments() {
    return d->segments;
}

void tTouchBarSegmentedControlItem::setCurrentSegment(int index) {
    d->currentSegment = index;
    this->updateItems();
}

int tTouchBarSegmentedControlItem::currentSegment() {
    return d->currentSegment;
}

#ifndef HAVE_TOUCHBAR_IMPL
NSView* tTouchBarSegmentedControlItem::view() {
    return nullptr;
}

void tTouchBarSegmentedControlItem::updateItems() {
}

void tTouchBarSegmentedControlItem::init() {
}

void tTouchBarSegmentedControlItem::tearDown() {
}
#endif
