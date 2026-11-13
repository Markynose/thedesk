#include "ttouchbarlabelitem.h"
#include "private/ttouchbarlabelitem_p.h"

void tTouchBarLabelItem::updateItem() {
    if (!d->label) {
        d->label = [[NSTextField labelWithString:this->text().toNSString()] retain];
    }
    [d->label setStringValue:this->text().toNSString()];

    emit invalidateTouchBar();
}

NSView *tTouchBarLabelItem::view() {
    this->updateItem();
    return d->label;
}

void tTouchBarLabelItem::tearDown() {
    if (d->label) {
        [d->label release];
    }
}
