#ifndef TCOMMANDPALETTEDOCUMENTSPECIFICSCOPE_H
#define TCOMMANDPALETTEDOCUMENTSPECIFICSCOPE_H

#include "libcontemporary_global.h"
#include "tcommandpalettecompositescope.h"

struct tCommandPaletteDocumentSpecificScopePrivate;
class LIBCONTEMPORARY_EXPORT tCommandPaletteDocumentSpecificScope : public tCommandPaletteCompositeScope {
        Q_OBJECT
    public:
        explicit tCommandPaletteDocumentSpecificScope(QObject* parent = nullptr);
        ~tCommandPaletteDocumentSpecificScope();

        void registerScope(QWidget* page, tCommandPaletteScope* scope);
        void deregisterScope(QWidget* page, tCommandPaletteScope* scope);

        template<typename T> struct Fail : std::false_type {};

    private:
        tCommandPaletteDocumentSpecificScopePrivate* d;

        // tCommandPaletteScope interface
    public:
        QString displayName();

        // tCommandPaletteCompositeScope interface
    public:
        template<typename T> void registerScope(tCommandPaletteScope* scope) {
            static_assert(Fail<T>::value, "Use a tCommandPaletteCompositeScope in order to register a scope without a page");
        }
        template<typename T> void deregisterScope(tCommandPaletteScope* scope) {
            static_assert(Fail<T>::value, "Use a tCommandPaletteCompositeScope in order to register a scope without a page");
        }
        QList<QPair<tCommandPaletteScope*, QModelIndex>> filteredItems(QString filter);
};

#endif // TCOMMANDPALETTEDOCUMENTSPECIFICSCOPE_H
