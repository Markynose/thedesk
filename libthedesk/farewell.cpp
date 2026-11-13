/****************************************
 *
 *   theDesk - the goodbye: shutting down, restarting, logging out
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "farewell.h"

#include "livingtypeview.h"
#include <QApplication>
#include <QCoreApplication>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>

namespace {
    bool playing = false;
    QList<QPointer<QWidget>> shown;

    QString firstName() {
        return LivingTypeView::userDisplayName().section(QLatin1Char(' '), 0, 0);
    }
} // namespace

QStringList Farewell::phrasesFor(Kind kind) {
    const QString name = firstName();
    auto tr = [](const char* text) {
        return QCoreApplication::translate("Farewell", text);
    };
    const QString goodbye = name.isEmpty() ? tr("Goodbye!") : tr("Goodbye, %1!").arg(name);
    switch (kind) {
        case Reboot:
            return {tr("Restarting..."), QStringLiteral("TheDesk"), tr("Back in a moment."), tr("See you on the other side!")};
        case LogOut:
            return {tr("Logging out..."), QStringLiteral("TheDesk"), goodbye, tr("See you soon, love!")};
        case PowerOff:
        default:
            return {tr("Shutting down..."), QStringLiteral("TheDesk"), goodbye, tr("Rest well, love!")};
    }
}

void Farewell::play(Kind kind, std::function<void()> then) {
    if (playing) return;
    playing = true;

    const QStringList phrases = phrasesFor(kind);
    QList<LivingTypeView*> views;
    QList<QPointer<QWidget>> windows;
    for (QScreen* screen : qApp->screens()) {
        // Unmanaged and on top, like the lock screen: the bar and the
        // Gateway can't come over it
        auto* window = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setWindowTitle(QStringLiteral("theDesk goodbye"));
        window->setGeometry(screen->geometry());
        auto* view = new LivingTypeView(window);
        view->setMode(LivingTypeView::Field);
        view->setGeometry(window->rect());
        view->setFarewell(phrases);
        window->show();
        window->raise();
        views.append(view);
        windows.append(window);
        shown.append(window);

        auto* keepOnTop = new QTimer(window);
        keepOnTop->setInterval(300);
        QObject::connect(keepOnTop, &QTimer::timeout, window, [window] {
            window->raise();
        });
        keepOnTop->start();
    }
    if (views.isEmpty()) {
        playing = false;
        then();
        return;
    }

    // The loading screen's words come up (as at login), a moment, then go
    QTimer::singleShot(1800, qApp, [then, windows] {
        // Stop raising it over everything: if the request needs a password,
        // its prompt has to come up above the goodbye
        for (const auto& window : windows) {
            if (!window) continue;
            for (QTimer* timer : window->findChildren<QTimer*>(Qt::FindDirectChildrenOnly)) timer->stop();
        }
        then();
        // Still here long after: it didn't happen (a nested test session, an
        // operation that never answered); give the desktop back. A refusal
        // takes it down at once (cancel)
        QTimer::singleShot(20000, qApp, [] {
            cancel();
        });
    });
}

void Farewell::cancel() {
    for (const auto& window : std::as_const(shown)) {
        if (window) window->close();
    }
    shown.clear();
    playing = false;
}
