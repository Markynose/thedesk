#ifndef TTOUCHBARPOPOVERITEM_H
#define TTOUCHBARPOPOVERITEM_H

#include "ttouchbarabstractitem.h"

class tTouchBar;
struct tTouchBarPopoverItemPrivate;
class tTouchBarPopoverItem : public tTouchBarAbstractItem {
        Q_OBJECT
    public:
        explicit tTouchBarPopoverItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarPopoverItem();

        void setCollapsedRepresentationLabel(QString collapsedRepresentationLabel);
        QString collapsedRepresentationLabel();

        void setPopoverTouchBar(tTouchBar* touchBar);
        tTouchBar* popoverTouchBar();

        void setIcon(QIcon icon, bool tint = true);
        QIcon icon();
        bool tintIcon();

        void closePopover();

    signals:

    private:
        tTouchBarPopoverItemPrivate* d;

        void updateItem();

        // tTouchBarAbstractItem interface
    public:
        NSTouchBarItem* makeItem();
};

#endif // TTOUCHBARPOPOVERITEM_H
