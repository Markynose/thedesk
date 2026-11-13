#include "ttouchbarpopoveritem.h"
#include "private/ttouchbarpopoveritem_p.h"

#include <libcontemporary_global.h>
#include "ttouchbar.h"

NSTouchBarItem* tTouchBarPopoverItem::makeItem() {
    this->updateItem();
    return d->touchBarItem;
}

void tTouchBarPopoverItem::updateItem() {
    if (!d->touchBarItem) {
        d->touchBarItem = [[NSPopoverTouchBarItem alloc] initWithIdentifier:this->identifier().toNSString()];
    }
    [d->touchBarItem setCustomizationLabel:this->customizationLabel().toNSString()];
    [d->touchBarItem setCollapsedRepresentationLabel:this->collapsedRepresentationLabel().toNSString()];

    auto button = (NSButton*)[d->touchBarItem view];
    if (!d->icon.isNull()) {
        auto image = this->icon().pixmap(QSize(64, 64)).toImage();
        if (this->tintIcon()) libContemporaryCommon::tintImage(image, Qt::white);
        auto cgImage = image.toCGImage();
        auto nsimage = [[NSImage alloc] initWithCGImage:cgImage size:(NSSize){16, 16}];
        [button setImage:nsimage];
    }

    if (d->popoverTouchBar) {
        [d->touchBarItem setPopoverTouchBar:d->popoverTouchBar->makeTouchBar()];
    }

    emit invalidateTouchBar();
}


void tTouchBarPopoverItem::closePopover()
{
    [d->touchBarItem dismissPopover:nil];
}
