#ifndef TTOUCHBARLABELITEM_H
#define TTOUCHBARLABELITEM_H

#include "ttouchbarcustomtouchbaritem.h"

struct tTouchBarLabelItemPrivate;
class tTouchBarLabelItem : public tTouchBarCustomTouchBarItem {
        Q_OBJECT
    public:
        explicit tTouchBarLabelItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarLabelItem();

        void setText(QString text);
        QString text();

    signals:

    private:
        tTouchBarLabelItemPrivate* d;

        void tearDown();

        void updateItem();

        // tTouchBarCustomTouchBarItem interface
    public:
        NSView* view();
};

#endif // TTOUCHBARLABELITEM_H
