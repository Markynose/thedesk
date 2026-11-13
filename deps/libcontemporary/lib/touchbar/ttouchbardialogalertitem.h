#ifndef TTOUCHBARDIALOGALERTITEM_H
#define TTOUCHBARDIALOGALERTITEM_H

#include "ttouchbargroupitem.h"

class tTouchBarButtonItem;
struct tTouchBarDialogAlertItemPrivate;
class tTouchBarDialogAlertItem : public tTouchBarAlertStyleGroupItem {
        Q_OBJECT
    public:
        explicit tTouchBarDialogAlertItem(QString identifier, QString customizationLabel, QString negativeButtonText, QString positiveButtonText, QObject* parent = nullptr);
        ~tTouchBarDialogAlertItem();

        tTouchBarButtonItem* negativeButton();
        tTouchBarButtonItem* positiveButton();

    signals:

    private:
        tTouchBarDialogAlertItemPrivate* d;
};

#endif // TTOUCHBARDIALOGALERTITEM_H
