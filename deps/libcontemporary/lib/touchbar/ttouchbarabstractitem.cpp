#include "ttouchbarabstractitem.h"

struct tTouchBarAbstractItemPrivate {
        QString identifier;
        QString customizationLabel;
        bool required = false;
        bool isPrincipal = false;
};

tTouchBarAbstractItem::tTouchBarAbstractItem(QString identifier, QString customizationLabel, QObject* parent) :
    QObject{parent} {
    d = new tTouchBarAbstractItemPrivate();
    d->identifier = identifier;
    d->customizationLabel = customizationLabel;
}

tTouchBarAbstractItem::~tTouchBarAbstractItem() {
    delete d;
}

QString tTouchBarAbstractItem::identifier() {
    return d->identifier;
}

QString tTouchBarAbstractItem::customizationLabel() {
    return d->customizationLabel;
}

void tTouchBarAbstractItem::setRequired(bool required) {
    d->required = required;
    emit invalidateTouchBar();
}

bool tTouchBarAbstractItem::required() {
    return d->required;
}

void tTouchBarAbstractItem::setIsPrincipal(bool isPrincipal) {
    d->isPrincipal = true;
    emit invalidateTouchBar();
}

bool tTouchBarAbstractItem::isPrincipal() {
    return d->isPrincipal;
}
