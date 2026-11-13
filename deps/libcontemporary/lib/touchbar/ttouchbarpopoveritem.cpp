#include "ttouchbarpopoveritem.h"
#include "private/ttouchbarpopoveritem_p.h"

tTouchBarPopoverItem::tTouchBarPopoverItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarAbstractItem{identifier, customizationLabel, parent} {
    d = new tTouchBarPopoverItemPrivate();
    d->collapsedRepresentationLabel = customizationLabel;
}

tTouchBarPopoverItem::~tTouchBarPopoverItem() {
    delete d;
}

void tTouchBarPopoverItem::setCollapsedRepresentationLabel(QString collapsedRepresentationLabel) {
    d->collapsedRepresentationLabel = collapsedRepresentationLabel;
    this->updateItem();
}

QString tTouchBarPopoverItem::collapsedRepresentationLabel() {
    return d->collapsedRepresentationLabel;
}

void tTouchBarPopoverItem::setPopoverTouchBar(tTouchBar* touchBar) {
    d->popoverTouchBar = touchBar;
    this->updateItem();
}

tTouchBar* tTouchBarPopoverItem::popoverTouchBar() {
    return d->popoverTouchBar;
}

void tTouchBarPopoverItem::setIcon(QIcon icon, bool tint) {
    d->icon = icon;
    d->tintIcon = tint;
    this->updateItem();
}

QIcon tTouchBarPopoverItem::icon() {
    return d->icon;
}

bool tTouchBarPopoverItem::tintIcon() {
    return d->tintIcon;
}

#ifndef HAVE_TOUCHBAR_IMPL
NSTouchBarItem* tTouchBarPopoverItem::makeItem() {
    return nullptr;
}

void tTouchBarPopoverItem::updateItem() {
}

void tTouchBarPopoverItem::closePopover() {
}
#endif
