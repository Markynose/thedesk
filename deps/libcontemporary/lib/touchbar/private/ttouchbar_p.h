#ifndef TTOUCHBAR_P_H
#define TTOUCHBAR_P_H

#include "../ttouchbar.h"
#include "../ttouchbar_forwarddeclares.h"

T_FORWARD_DECLARE_OBJC_INTERFACE(TTouchBarMainWindowDelegate);

struct tTouchBarPrivate {
        NSTouchBar* touchBar;
        TTouchBarMainWindowDelegate* delegate;

        QList<tTouchBarAbstractItem*> touchBarItems;
        QList<tTouchBarAbstractItem*> defaultTouchBarItems;

        QString customizationIdentifier;
};

#endif // TTOUCHBAR_P_H
