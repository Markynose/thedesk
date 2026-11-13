#include "ttouchbarlabelitem.h"
#include "private/ttouchbarlabelitem_p.h"

tTouchBarLabelItem::tTouchBarLabelItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarCustomTouchBarItem{identifier, customizationLabel, parent} {
    d = new tTouchBarLabelItemPrivate();
    d->text = customizationLabel;
}

tTouchBarLabelItem::~tTouchBarLabelItem() {
    delete d;
}

void tTouchBarLabelItem::setText(QString text) {
    d->text = text;
    this->updateItem();
}

QString tTouchBarLabelItem::text() {
    return d->text;
}

#ifndef HAVE_TOUCHBAR_IMPL
void tTouchBarLabelItem::updateItem() {
}

NSView* tTouchBarLabelItem::view() {
    return nullptr;
}

void tTouchBarLabelItem::tearDown() {
}
#endif
