#include "ttouchbardialogalertitem.h"

#include "ttouchbar.h"
#include "ttouchbarbuttonitem.h"

struct tTouchBarDialogAlertItemPrivate {
        tTouchBarButtonItem* negativeButton;
        tTouchBarButtonItem* positiveButton;
};

tTouchBarDialogAlertItem::tTouchBarDialogAlertItem(QString identifier, QString customizationLabel, QString negativeButtonText, QString positiveButtonText, QObject* parent) :
    tTouchBarAlertStyleGroupItem{identifier, customizationLabel, parent} {
    d = new tTouchBarDialogAlertItemPrivate();

    this->setIsPrincipal(true);
    auto groupTouchBar = new tTouchBar(this);

    d->negativeButton = new tTouchBarButtonItem(QStringLiteral("%1.cancel").arg(identifier), negativeButtonText, this);
    groupTouchBar->addDefaultItem(d->negativeButton);

    d->positiveButton = new tTouchBarButtonItem(QStringLiteral("%1.ok").arg(identifier), positiveButtonText, this);
    d->positiveButton->setIsPrimary(true);
    groupTouchBar->addDefaultItem(d->positiveButton);
    this->setGroupTouchBar(groupTouchBar);
}

tTouchBarDialogAlertItem::~tTouchBarDialogAlertItem() {
    delete d;
}

tTouchBarButtonItem* tTouchBarDialogAlertItem::negativeButton() {
    return d->negativeButton;
}

tTouchBarButtonItem* tTouchBarDialogAlertItem::positiveButton() {
    return d->positiveButton;
}
