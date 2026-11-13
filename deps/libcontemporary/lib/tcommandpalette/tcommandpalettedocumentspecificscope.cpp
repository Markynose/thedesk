#include "tcommandpalettedocumentspecificscope.h"

struct tCommandPaletteDocumentSpecificScopePrivate {
};

tCommandPaletteDocumentSpecificScope::tCommandPaletteDocumentSpecificScope(QObject* parent) :
    tCommandPaletteCompositeScope("", parent) {
    d = new tCommandPaletteDocumentSpecificScopePrivate();
}

tCommandPaletteDocumentSpecificScope::~tCommandPaletteDocumentSpecificScope() {
    delete d;
}

void tCommandPaletteDocumentSpecificScope::registerScope(QWidget* page, tCommandPaletteScope* scope) {
    tCommandPaletteCompositeScope::registerScope(scope);
}

void tCommandPaletteDocumentSpecificScope::deregisterScope(QWidget* page, tCommandPaletteScope* scope) {
    tCommandPaletteCompositeScope::deregisterScope(scope);
}

QString tCommandPaletteDocumentSpecificScope::displayName() {
    if (scopes().isEmpty()) return "";
    return scopes().constFirst()->displayName();
}

QList<QPair<tCommandPaletteScope*, QModelIndex>> tCommandPaletteDocumentSpecificScope::filteredItems(QString filter) {
    auto items = tCommandPaletteCompositeScope::filteredItems(filter);
    return items;
}
