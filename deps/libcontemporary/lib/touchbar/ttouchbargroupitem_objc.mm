#include "ttouchbargroupitem.h"
#include "private/ttouchbargroupitem_p.h"

#include "ttouchbar.h"

NSTouchBarItem *tTouchBarGroupItem::makeItem()
{
    this->updateItem();
    return d->touchBarItem;
}

void tTouchBarGroupItem::updateItem()
{
    if (!d->touchBarItem) {
        d->touchBarItem = [[NSGroupTouchBarItem alloc] initWithIdentifier:this->identifier().toNSString()];
    }
    [d->touchBarItem setCustomizationLabel:this->customizationLabel().toNSString()];

    if (d->groupTouchBar) {
        [d->touchBarItem setGroupTouchBar:d->groupTouchBar->makeTouchBar()];
    }

    emit invalidateTouchBar();
}

void tTouchBarAlertStyleGroupItem::updateItem()
{
    if (!d->touchBarItem) {
        d->touchBarItem = [NSGroupTouchBarItem alertStyleGroupItemWithIdentifier:this->identifier().toNSString()];
    }

    tTouchBarGroupItem::updateItem();
}
