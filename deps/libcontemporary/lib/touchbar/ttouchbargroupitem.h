#ifndef TTOUCHBARGROUPITEM_H
#define TTOUCHBARGROUPITEM_H

#include "ttouchbarabstractitem.h"

class tTouchBar;
struct tTouchBarGroupItemPrivate;
class tTouchBarGroupItem : public tTouchBarAbstractItem {
        Q_OBJECT
    public:
        explicit tTouchBarGroupItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarGroupItem();

        void setGroupTouchBar(tTouchBar* touchBar);
        tTouchBar* groupTouchBar();

    signals:

    protected:
        tTouchBarGroupItemPrivate* d;

        void updateItem();

        // tTouchBarAbstractItem interface
    public:
        NSTouchBarItem* makeItem();
};

class tTouchBarAlertStyleGroupItem : public tTouchBarGroupItem {
        Q_OBJECT

    public:
        explicit tTouchBarAlertStyleGroupItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarAlertStyleGroupItem();

    private:
        void updateItem();
};

#endif // TTOUCHBARGROUPITEM_H
