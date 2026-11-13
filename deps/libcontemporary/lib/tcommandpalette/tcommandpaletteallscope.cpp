#include "tcommandpaletteallscope.h"

#include "tcommandpalettecontroller.h"

struct tCommandPaletteAllScopePrivate {
        tCommandPaletteController* controller;
};

tCommandPaletteAllScope::tCommandPaletteAllScope(tCommandPaletteController* controller, QObject* parent) :
    tCommandPaletteCompositeScope(tr("All"), parent) {
    d = new tCommandPaletteAllScopePrivate();
    d->controller = controller;

    connect(d->controller, &tCommandPaletteController::scopesChanged, this, &tCommandPaletteAllScope::updateScopes);
    this->updateScopes();
}

tCommandPaletteAllScope::~tCommandPaletteAllScope() {
    delete d;
}

void tCommandPaletteAllScope::updateScopes() {
    for (auto* scope : d->controller->scopes()) {
        if (!scope) continue;
        if (scope == this) continue;
        if (scopeRegistered(scope)) continue;

        registerScope(scope);
    }
}

QList<QPair<tCommandPaletteScope*, QModelIndex>> tCommandPaletteAllScope::filteredItems(QString filter) {
    if (filter.isEmpty()) return {};
    return tCommandPaletteCompositeScope::filteredItems(filter);
}
