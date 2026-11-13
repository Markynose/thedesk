#ifndef TCOMMANDPALETTECONTROLLER_H
#define TCOMMANDPALETTECONTROLLER_H

#include "libcontemporary_global.h"
#include <QObject>

class QAction;
class tCommandPaletteScope;
class tCommandPaletteActionScope;
struct tCommandPaletteControllerPrivate;
class LIBCONTEMPORARY_EXPORT tCommandPaletteController : public QObject {
        Q_OBJECT
    public:
        explicit tCommandPaletteController(QWidget* parent);
        ~tCommandPaletteController();

        static tCommandPaletteController* defaultController(QWidget* parent, tCommandPaletteActionScope** actionScope);

        void addScope(tCommandPaletteScope* scope);
        QList<tCommandPaletteScope*> scopes();

        void setCurrentQuery(QString query);
        QString currentQuery();

        void activate();
        void installShortcut();

        QAction* commandPaletteAction();

    signals:
        void scopesChanged();

    private:
        tCommandPaletteControllerPrivate* d;

        // QObject interface
    public:
        bool eventFilter(QObject* watched, QEvent* event);
};

#endif // TCOMMANDPALETTECONTROLLER_H
