#ifndef TTOUCHBARBUTTONITEM_P_H
#define TTOUCHBARBUTTONITEM_P_H

#include "../ttouchbar_forwarddeclares.h"
#include <QIcon>
#include <QString>

T_FORWARD_DECLARE_OBJC_INTERFACE(TTouchBarButtonItemInternalHandler);
T_FORWARD_DECLARE_OBJC_INTERFACE(NSButton);

struct tTouchBarButtonItemPrivate {
        QString text;
        QIcon icon;
        bool tint = true;
        bool enabled = true;
        bool visible = true;
        bool isPrimary = false;

        TTouchBarButtonItemInternalHandler* internalHandler;
        NSButton* touchBarButton = nullptr;
};

#endif // TTOUCHBARBUTTONITEM_P_H
