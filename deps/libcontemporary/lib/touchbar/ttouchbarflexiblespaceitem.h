#ifndef TTOUCHBARFLEXIBLESPACEITEM_H
#define TTOUCHBARFLEXIBLESPACEITEM_H

#include "ttouchbarabstractitem.h"

class tTouchBarFlexibleSpaceItem : public tTouchBarAbstractItem {
        Q_OBJECT
    public:
        explicit tTouchBarFlexibleSpaceItem(QObject* parent = nullptr);

    signals:

        // tTouchBarAbstractItem interface
    public:
        NSTouchBarItem* makeItem();
};

#endif // TTOUCHBARFLEXIBLESPACEITEM_H
