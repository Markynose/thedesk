#ifndef TTOUCHBARSEGMENTEDCONTROLITEM_P_H
#define TTOUCHBARSEGMENTEDCONTROLITEM_P_H

#include "../ttouchbar_forwarddeclares.h"
#include <QStringList>

T_FORWARD_DECLARE_OBJC_INTERFACE(NSSegmentedControl);
T_FORWARD_DECLARE_OBJC_INTERFACE(TTouchBarSegmentedControlItemInternalHandler);

struct tTouchBarSegmentedControlItemPrivate {
        QStringList segments;
        int currentSegment = 0;

        TTouchBarSegmentedControlItemInternalHandler* internalHandler;
        NSSegmentedControl* segmentedControl = nullptr;
};

#endif // TTOUCHBARSEGMENTEDCONTROLITEM_P_H
