#include "tcommandpalettecontroller.h"

#include "tcommandpaletteactionscope.h"
#include "tcommandpaletteallscope.h"
#include "tcommandpalettewindow.h"
#include <QAction>
#include <QKeyEvent>
#include <QPointer>
#include <QShortcut>
#include <QWindow>

struct tCommandPaletteControllerPrivate {
        QWidget* parent;
        QPointer<tCommandPaletteWindow> activeWindow;
        QAction* commandPaletteAction;

        QList<tCommandPaletteScope*> scopes;
        tCommandPaletteScope* allScope;

        QString currentQuery;

        bool awaitingNextShift = false;
};

tCommandPaletteController::tCommandPaletteController(QWidget* parent) :
    QObject{parent} {
    d = new tCommandPaletteControllerPrivate();
    d->parent = parent;

    d->commandPaletteAction = new QAction();
    d->commandPaletteAction->setText(tr("Command Palette"));
    d->commandPaletteAction->setShortcut(QKeySequence(Qt::Key_Shift, Qt::Key_Shift));
    d->commandPaletteAction->setMenuRole(QAction::ApplicationSpecificRole);
    connect(d->commandPaletteAction, &QAction::triggered, this, &tCommandPaletteController::activate);

    d->allScope = new tCommandPaletteAllScope(this);
}

tCommandPaletteController::~tCommandPaletteController() {
    delete d;
}

tCommandPaletteController* tCommandPaletteController::defaultController(QWidget* parent, tCommandPaletteActionScope** actionScope) {
    auto controller = new tCommandPaletteController(parent);
    controller->installShortcut();
    *actionScope = new tCommandPaletteActionScope(controller, controller->commandPaletteAction());
    controller->addScope(*actionScope);
    return controller;
}

void tCommandPaletteController::addScope(tCommandPaletteScope* scope) {
    d->scopes.append(scope);
    emit scopesChanged();
}

QList<tCommandPaletteScope*> tCommandPaletteController::scopes() {
    auto scopes = d->scopes;
    if (scopes.length() != 1) scopes.prepend(d->allScope);
    return scopes;
}

void tCommandPaletteController::setCurrentQuery(QString query) {
    d->currentQuery = query;
}

QString tCommandPaletteController::currentQuery() {
    return d->currentQuery;
}

void tCommandPaletteController::activate() {
    if (!d->activeWindow) {
        d->activeWindow = new tCommandPaletteWindow(this, d->parent);
    }

    d->activeWindow->show();
    d->activeWindow->activateWindow();
    d->activeWindow->raise();

    QRect geometry;
    geometry.setSize(QSize(400, 600));
    geometry.moveCenter(d->parent->geometry().center());
    d->activeWindow->setGeometry(geometry);
}

void tCommandPaletteController::installShortcut() {
    //    auto shortcut = new QShortcut(QKeySequence(Qt::Key_Shift, Qt::Key_Shift), d->parent);
    //    connect(shortcut, &QShortcut::activatedAmbiguously, this, &tCommandPaletteController::activate);
    qApp->installEventFilter(this);
}

QAction* tCommandPaletteController::commandPaletteAction() {
    return d->commandPaletteAction;
}

bool tCommandPaletteController::eventFilter(QObject* watched, QEvent* event) {
    if ((d->parent && watched == d->parent->windowHandle()) && event->type() == QEvent::KeyPress) {
        auto ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Shift) {
            if (d->awaitingNextShift) {
                d->awaitingNextShift = false;
                this->activate();
            } else {
                d->awaitingNextShift = true;
            }
        } else {
            d->awaitingNextShift = false;
        }
    } else if (event->type() == QEvent::KeyPress) {
        auto ke = static_cast<QKeyEvent*>(event);
        if (ke->key() != Qt::Key_Shift) {
            d->awaitingNextShift = false;
        }
    }
    return false;
}
