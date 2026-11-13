#include "ttouchbarflexiblespaceitem.h"

tTouchBarFlexibleSpaceItem::tTouchBarFlexibleSpaceItem(QObject* parent) :
    tTouchBarAbstractItem{QString::fromNSString(NSTouchBarItemIdentifierFlexibleSpace), QStringLiteral(""), parent} {
}
