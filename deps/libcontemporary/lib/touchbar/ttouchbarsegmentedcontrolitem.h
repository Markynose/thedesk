#ifndef TTOUCHBARSEGMENTEDCONTROLITEM_H
#define TTOUCHBARSEGMENTEDCONTROLITEM_H

#include "ttouchbarcustomtouchbaritem.h"

struct tTouchBarSegmentedControlItemPrivate;
class tTouchBarSegmentedControlItem : public tTouchBarCustomTouchBarItem {
        Q_OBJECT
    public:
        explicit tTouchBarSegmentedControlItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarSegmentedControlItem();

        void setSegments(QStringList segments);
        QStringList segments();

        void setCurrentSegment(int index);
        int currentSegment();

    signals:
        void currentSegmentChanged(int index);

    private:
        tTouchBarSegmentedControlItemPrivate* d;

        void init();
        void tearDown();

        void updateItems();

        // tTouchBarCustomTouchBarItem interface
    public:
        NSView* view();
};

#endif // TTOUCHBARSEGMENTEDCONTROLITEM_H
