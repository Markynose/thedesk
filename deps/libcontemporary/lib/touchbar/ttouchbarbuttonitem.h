#ifndef TTOUCHBARBUTTONITEM_H
#define TTOUCHBARBUTTONITEM_H

#include "ttouchbarcustomtouchbaritem.h"

struct tTouchBarButtonItemPrivate;
class tTouchBarButtonItem : public tTouchBarCustomTouchBarItem {
        Q_OBJECT
    public:
        explicit tTouchBarButtonItem(QString identifier, QString customizationLabel, QObject* parent = nullptr);
        ~tTouchBarButtonItem();

        void setText(QString text);
        QString text();

        void setIcon(QIcon icon, bool tint = true);
        QIcon icon();
        bool tintIcon();

        void setEnabled(bool enabled);
        bool enabled();

        void setVisible(bool visible);
        bool visible();

        void setIsPrimary(bool isPrimary);
        bool isPrimary();

    signals:
        void clicked();

    private:
        tTouchBarButtonItemPrivate* d;

        void init();
        void tearDown();
        void updateTouchBarItem();

        // tTouchBarCustomTouchBarItem interface
    public:
        NSView* view();
};

#endif // TTOUCHBARBUTTONITEM_H
