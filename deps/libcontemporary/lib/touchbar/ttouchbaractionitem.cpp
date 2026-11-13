#include "ttouchbaractionitem.h"

#include <QAction>

struct tTouchBarActionItemPrivate {
        QAction* action;
        QIcon icon;
};

tTouchBarActionItem::tTouchBarActionItem(QString identifier, QAction* action, QString customizationLabel, QIcon icon, QObject* parent) :
    tTouchBarButtonItem{identifier, customizationLabel.isEmpty() ? action->text() : customizationLabel, parent} {
    d = new tTouchBarActionItemPrivate();
    d->action = action;
    d->icon = icon;

    connect(action, &QAction::changed, this, &tTouchBarActionItem::updateItem);
    connect(action, &QAction::enabledChanged, this, &tTouchBarActionItem::setEnabled);
    connect(this, &tTouchBarActionItem::clicked, action, &QAction::trigger);

    this->updateItem();
}

tTouchBarActionItem::~tTouchBarActionItem() {
    delete d;
}

void tTouchBarActionItem::setTouchBarIcon(QIcon touchBarIcon) {
    d->icon = touchBarIcon;
    this->updateItem();
}

QIcon tTouchBarActionItem::touchBarIcon() {
    return d->icon;
}

void tTouchBarActionItem::updateItem() {
    this->setText(d->action->text());
    this->setIcon(d->icon.isNull() ? d->action->icon() : d->icon);
}
