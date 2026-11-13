#include "ttouchbarbuttonitem.h"
#include "private/ttouchbarbuttonitem_p.h"

tTouchBarButtonItem::tTouchBarButtonItem(QString identifier, QString customizationLabel, QObject* parent) :
    tTouchBarCustomTouchBarItem{identifier, customizationLabel, parent} {
    d = new tTouchBarButtonItemPrivate();
    d->text = customizationLabel;

    this->init();
}

tTouchBarButtonItem::~tTouchBarButtonItem() {
    delete d;
}

void tTouchBarButtonItem::setText(QString text) {
    d->text = text;
    this->updateTouchBarItem();
}

QString tTouchBarButtonItem::text() {
    return d->text;
}

void tTouchBarButtonItem::setIcon(QIcon icon, bool tint) {
    d->icon = icon;
    d->tint = tint;
    this->updateTouchBarItem();
}

QIcon tTouchBarButtonItem::icon() {
    return d->icon;
}

bool tTouchBarButtonItem::tintIcon() {
    return d->tint;
}

void tTouchBarButtonItem::setEnabled(bool enabled) {
    d->enabled = enabled;
    this->updateTouchBarItem();
}

bool tTouchBarButtonItem::enabled() {
    return d->enabled;
}

void tTouchBarButtonItem::setVisible(bool visible) {
    d->visible = visible;
    this->updateTouchBarItem();
}

bool tTouchBarButtonItem::visible() {
    return d->visible;
}

void tTouchBarButtonItem::setIsPrimary(bool isPrimary) {
    d->isPrimary = isPrimary;
    this->updateTouchBarItem();
}

bool tTouchBarButtonItem::isPrimary() {
    return d->isPrimary;
}

#ifndef HAVE_TOUCHBAR_IMPL
void tTouchBarButtonItem::init() {
}

void tTouchBarButtonItem::tearDown() {
}

void tTouchBarButtonItem::updateTouchBarItem() {
    emit invalidateTouchBar();
}

NSView* tTouchBarButtonItem::view() {
    return nullptr;
}
#endif
