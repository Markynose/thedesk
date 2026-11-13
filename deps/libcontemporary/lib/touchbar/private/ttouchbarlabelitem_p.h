#ifndef TTOUCHBARLABELITEM_P_H
#define TTOUCHBARLABELITEM_P_H

#include "../ttouchbar_forwarddeclares.h"
#include <QString>

T_FORWARD_DECLARE_OBJC_INTERFACE(NSTextField);

struct tTouchBarLabelItemPrivate {
        QString text;

        NSTextField* label = nullptr;
};

#endif // TTOUCHBARLABELITEM_P_H
