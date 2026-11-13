#ifndef TTOUCHBARGROUPITEM_P_H
#define TTOUCHBARGROUPITEM_P_H

#include "../ttouchbar_forwarddeclares.h"

T_FORWARD_DECLARE_OBJC_INTERFACE(NSGroupTouchBarItem);

class tTouchBar;
struct tTouchBarGroupItemPrivate {
        tTouchBar* groupTouchBar = nullptr;
        NSGroupTouchBarItem* touchBarItem = nullptr;
};

#endif // TTOUCHBARGROUPITEM_P_H
