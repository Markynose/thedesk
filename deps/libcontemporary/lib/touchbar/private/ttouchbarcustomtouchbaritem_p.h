#ifndef TTOUCHBARCUSTOMTOUCHBARITEM_P_H
#define TTOUCHBARCUSTOMTOUCHBARITEM_P_H

#include "../ttouchbar_forwarddeclares.h"
#include <QString>

T_FORWARD_DECLARE_OBJC_INTERFACE(NSCustomTouchBarItem);

struct tTouchBarCustomTouchBarItemPrivate {
        NSCustomTouchBarItem* touchBarItem = nullptr;
};

#endif
