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
#ifndef FAREWELL_H
#define FAREWELL_H

#include "libthedesk_global.h"
#include <QStringList>
#include <functional>

/**
 * The goodbye: the same loading screen as at login, with goodbye words in
 * place of the loading ones; it stays up while the session ends (until the X
 * server goes, or theDesk quits).
 */
namespace Farewell {
    enum Kind {
        PowerOff,
        Reboot,
        LogOut
    };

    LIBTHEDESK_EXPORT QStringList phrasesFor(Kind kind);

    // Covers every screen and calls `then` once the words have come up.
    // Calling it again while it plays is a no-op.
    LIBTHEDESK_EXPORT void play(Kind kind, std::function<void()> then);

    // The operation didn't happen (refused, failed): take the goodbye down
    // now and give the desktop back
    LIBTHEDESK_EXPORT void cancel();
} // namespace Farewell

#endif // FAREWELL_H
