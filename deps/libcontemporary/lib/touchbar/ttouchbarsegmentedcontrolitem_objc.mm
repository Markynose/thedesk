#include "ttouchbarsegmentedcontrolitem.h"
#include "private/ttouchbarsegmentedcontrolitem_p.h"

@interface TTouchBarSegmentedControlItemInternalHandler : NSObject
@property tTouchBarSegmentedControlItem* parentItem;
@end

@implementation TTouchBarSegmentedControlItemInternalHandler
- (id)init: (tTouchBarSegmentedControlItem*)parentItem {
    if (self = [super init]) {
        self.parentItem = parentItem;
    }
    return self;
}

- (void)currentSegmentChanged: (NSSegmentedControl*) segmentedControl {
    emit self.parentItem->currentSegmentChanged(segmentedControl.selectedSegment);
}
@end

NSView* tTouchBarSegmentedControlItem::view() {
    this->updateItems();
    return d->segmentedControl;
}

void tTouchBarSegmentedControlItem::updateItems()
{
    if (!d->segmentedControl) {
        d->segmentedControl = [[NSSegmentedControl alloc] init];
        [d->segmentedControl retain];

        [d->segmentedControl setTarget:d->internalHandler];
        [d->segmentedControl setAction:@selector(currentSegmentChanged:)];
    }


    [d->segmentedControl setSegmentCount:d->segments.length()];
    for (auto i = 0; i < d->segments.length(); i++) {
        auto segment = d->segments.at(i);
        [d->segmentedControl setLabel:segment.toNSString() forSegment:i];
    }
    [d->segmentedControl setSelectedSegment:d->currentSegment];

    emit invalidateTouchBar();
}

void tTouchBarSegmentedControlItem::init()
{
    d->internalHandler = [[TTouchBarSegmentedControlItemInternalHandler alloc] init:this];
}

void tTouchBarSegmentedControlItem::tearDown()
{
    if (d->segmentedControl) {
        [d->segmentedControl release];
    }
}
