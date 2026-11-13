#include "tcommandpaletteactionscope.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>

struct tCommandPaletteActionScopePrivate {
        QAction* exclusion;
        QList<QAction*> actions;
        QList<QAction*> displayedActions;
};

tCommandPaletteActionScope::tCommandPaletteActionScope(QObject* parent, QAction* exclusion) :
    tCommandPaletteScope{parent} {
    d = new tCommandPaletteActionScopePrivate();
    d->exclusion = exclusion;
}

tCommandPaletteActionScope::~tCommandPaletteActionScope() {
    delete d;
}

void tCommandPaletteActionScope::addAction(QAction* action) {
    if (action == d->exclusion) return;
    if (action->isSeparator()) return;
    d->actions.append(action);
    filter("");
}

void tCommandPaletteActionScope::addMenu(QMenu* menu) {
    for (auto action : menu->actions()) {
        this->addAction(action);
    }
}

void tCommandPaletteActionScope::addMenuBar(QMenuBar* menuBar) {
    for (auto menu : menuBar->actions()) {
        this->addMenu(menu->menu());
    }
}

QString tCommandPaletteActionScope::displayName() {
    return tr("Actions");
}

int tCommandPaletteActionScope::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return d->displayedActions.length();
}

QVariant tCommandPaletteActionScope::data(const QModelIndex& index, int role) const {
    auto action = d->displayedActions.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
            return action->text();
        case Qt::DecorationRole:
            return action->icon();
    }

    return QVariant();
}

void tCommandPaletteActionScope::filter(QString filter) {
    this->beginResetModel();
    if (filter.isEmpty()) {
        d->displayedActions = d->actions;
        this->endResetModel();
        return;
    }

    d->displayedActions.clear();
    auto lowerFilter = filter.toLower();
    for (auto action : d->actions) {
        if (action->text().toLower().contains(lowerFilter)) {
            d->displayedActions.append(action);
        }
    }
    this->endResetModel();
}

void tCommandPaletteActionScope::activate(QModelIndex index) {
    auto action = d->displayedActions.at(index.row());
    if (action->isEnabled()) action->trigger();
}

Qt::ItemFlags tCommandPaletteActionScope::flags(const QModelIndex& index) const {
    auto action = d->displayedActions.at(index.row());
    auto flags = tCommandPaletteScope::flags(index);
    if (!action->isEnabled()) flags &= ~Qt::ItemIsEnabled;
    return flags;
}
