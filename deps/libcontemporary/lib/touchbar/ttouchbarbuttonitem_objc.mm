#include "ttouchbarbuttonitem.h"
#include "private/ttouchbarbuttonitem_p.h"

#include <libcontemporary_global.h>

@interface TTouchBarButtonItemInternalHandler : NSObject
@property tTouchBarButtonItem* parentItem;
@end

@implementation TTouchBarButtonItemInternalHandler
- (id)init: (tTouchBarButtonItem*)parentItem {
    if (self = [super init]) {
        self.parentItem = parentItem;
    }
    return self;
}

- (void)touchBarActionClicked: (NSButton*) button {
    emit self.parentItem->clicked();
}
@end

NSView* tTouchBarButtonItem::view() {
    this->updateTouchBarItem();
    return d->touchBarButton;
}

void tTouchBarButtonItem::init()
{
    d->internalHandler = [[TTouchBarButtonItemInternalHandler alloc] init:this];
}

void tTouchBarButtonItem::tearDown()
{
    if (d->touchBarButton) {
        [d->touchBarButton release];
    }
}

void tTouchBarButtonItem::updateTouchBarItem()
{
    if (!d->touchBarButton) {
        d->touchBarButton = [NSButton buttonWithTitle:this->text().toNSString() target:d->internalHandler action:@selector(touchBarActionClicked:)];
        [d->touchBarButton retain];
    }
    [d->touchBarButton setTitle:this->text().toNSString()];
    [d->touchBarButton setIdentifier:this->identifier().toNSString()];

    if (!this->icon().isNull()) {
        auto image = this->icon().pixmap(QSize(64, 64)).toImage();
        if (this->tintIcon()) libContemporaryCommon::tintImage(image, Qt::white);
        auto cgImage = image.toCGImage();
        auto nsimage = [[NSImage alloc] initWithCGImage:cgImage size:(NSSize){16, 16}];
        [d->touchBarButton setImage:nsimage];
        [d->touchBarButton setImagePosition:NSImageOnly];
    } else {
        [d->touchBarButton setImagePosition:NSNoImage];
    }

    [d->touchBarButton setEnabled:this->enabled() ? YES : NO];
    [d->touchBarButton setHidden:this->visible() ? NO : YES];

    if (this->isPrimary()) {
        [d->touchBarButton setKeyEquivalent:@"\r"];
    } else {
        [d->touchBarButton setKeyEquivalent:@""];
    }

    emit invalidateTouchBar();
}
