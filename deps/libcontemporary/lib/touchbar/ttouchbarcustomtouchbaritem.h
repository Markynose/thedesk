#ifndef TTOUCHBARCUSTOMTOUCHBARITEM_H
#define TTOUCHBARCUSTOMTOUCHBARITEM_H

#include "ttouchbarabstractitem.h"

struct tTouchBarCustomTouchBarItemPrivate;
class tTouchBarCustomTouchBarItem : public tTouchBarAbstractItem {
        Q_OBJECT
    public:
        explicit tTouchBarCustomTouchBarItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarCustomTouchBarItem();

        virtual NSView* view() = 0;

    signals:

    private:
        tTouchBarCustomTouchBarItemPrivate* d;

        // tTouchBarAbstractItem interface
    public:
        NSTouchBarItem* makeItem() final;
};

#endif // TTOUCHBARCUSTOMTOUCHBARITEM_H
