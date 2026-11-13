/****************************************
 *
 *   theDesk - Material 3 themed app icons
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3THEMEDICON_H
#define M3THEMEDICON_H

#include "libthedesk_global.h"
#include <QIcon>
#include <QImage>

/**
 * Themed app icons, in the spirit of Android's "Themed icons": Linux apps
 * have no monochrome icon layer, so the real icon is redrawn as a duotone in
 * the M3 scheme (its brightness mapped from a mid tone to OnPrimaryContainer)
 * on a PrimaryContainer circle. Used where theDesk shows apps to launch or
 * switch to (Gateway, taskbar); windows keep their own icons.
 *
 * Appearance/themedIcons=false in theDesk.conf shows the icons as they are.
 */
class LIBTHEDESK_EXPORT M3ThemedIcon {
    public:
        static bool enabled();

        // The themed rendering of `source` at `size` (logical px) and `dpr`
        static QImage render(const QImage& source, QSize size, qreal dpr = 1);
        // A QIcon that draws `source` themed (or `source` itself when off)
        static QIcon icon(const QIcon& source);

        // Just the duotone picture, for a container the caller draws itself
        // (a MaterialShape that moves): dark parts sink toward `container`,
        // bright ones rise to `content`; the icon keeps its own alpha
        static QImage glyph(const QImage& source, const QColor& container, const QColor& content);
};

#endif // M3THEMEDICON_H
