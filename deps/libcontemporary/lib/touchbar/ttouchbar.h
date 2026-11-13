#ifndef TTOUCHBAR_H
#define TTOUCHBAR_H

#include "ttouchbar_forwarddeclares.h"
#include <QObject>

T_FORWARD_DECLARE_OBJC_INTERFACE(NSTouchBar);

class tTouchBarAbstractItem;
struct tTouchBarPrivate;
class tTouchBarPopoverItem;
class tTouchBarGroupItem;
class QAction;
class QMenu;
class tTouchBar : public QObject {
        Q_OBJECT
    public:
        explicit tTouchBar(QObject* parent = nullptr);
        ~tTouchBar();

        static void addCustomizationMenuItem(QMenu* menu);

        virtual void attach(QWidget* widget);
        virtual void detach();

        void setCustomizationIdentifier(QString customizationIdentifier);
        QString customizationIdentifier();

        void addItem(tTouchBarAbstractItem* item);
        QList<tTouchBarAbstractItem*> touchBarItems();

        void addDefaultItem(tTouchBarAbstractItem* item);
        QList<tTouchBarAbstractItem*> defaultItems();

    signals:

    protected:
        friend tTouchBarPopoverItem;
        friend tTouchBarGroupItem;
        NSTouchBar* makeTouchBar();

    private:
        tTouchBarPrivate* d;

        void init();
        void invalidateTouchBar();
};

#endif // TTOUCHBAR_H
