#ifndef TTOUCHBARPOPOVERITEM_P_H
#define TTOUCHBARPOPOVERITEM_P_H

#include "../ttouchbar_forwarddeclares.h"
#include <QIcon>
#include <QString>

T_FORWARD_DECLARE_OBJC_INTERFACE(NSPopoverTouchBarItem);

class tTouchBar;
struct tTouchBarPopoverItemPrivate {
        QString collapsedRepresentationLabel;
        QIcon icon;
        bool tintIcon;

        tTouchBar* popoverTouchBar = nullptr;
        NSPopoverTouchBarItem* touchBarItem = nullptr;
};

#endif // TTOUCHBARPOPOVERITEM_P_H
