/****************************************
 *
 *   theDesk - Material 3 Expressive pointer
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3CURSOR_H
#define M3CURSOR_H

#include <QColor>
#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QStringList>

/**
 * The theDesk-M3 cursor theme: a rounded arrow (corners from graphics-shapes
 * RoundedPolygon), an I-beam, and the M3 loading indicator for busy cursors.
 *
 * tools/m3-cursors turns these into an Xcursor theme; the widget style draws
 * the same arrow at other scales for the hover-merge, where the pointer melts
 * into the button it touches.
 *
 * All geometry is in a 24 x 24 design box; images are rendered at `size`
 * pixels (24 = 1x).
 */
namespace M3Cursor {
    inline constexpr const char* ThemeName = "theDesk-M3";
    inline constexpr double DesignSize = 24;

    // Platform/cursor is theDesk-M3 (only then does the style animate the pointer)
    bool active();
    // Platform/cursorSize, else XCURSOR_SIZE, else 24
    int nominalSize();

    struct Colours {
            QColor fill;
            QColor keyline; // the outline that keeps it readable on anything
    };
    Colours neutralColours(); // OnSurface ink, white keyline (the installed theme)
    Colours schemeColours();  // Primary with an OnPrimary keyline, from the session's M3 scheme

    QPainterPath arrowPath();
    QPointF arrowHotspot(); // the tip, in design units

    // The arrow scaled by `scale` about its tip (0 = gone); premultiplied ARGB
    QImage renderArrow(int size, const Colours& colours, double scale = 1);
    QImage renderText(int size, const Colours& colours);
    QPointF textHotspot();
    // The loading indicator `ms` into its loop; withArrow = the "progress"
    // cursor (arrow plus a small indicator), otherwise "wait"
    QImage renderBusy(int size, const Colours& colours, qint64 ms, bool withArrow);
    QPointF busyHotspot(bool withArrow);

    // The rest of the standard cursors (pointer, grab, move, resizes,
    // crosshair, not-allowed, help, zoom, copy/link/context-menu...): filled
    // Material Symbols Rounded glyphs inked like the arrow, or the arrow with
    // a symbol badge. Names are the freedesktop/CSS cursor names
    QStringList extraCursorNames();
    QImage renderExtra(const QString& name, int size, const Colours& colours, QPointF* hotspot = nullptr);

    // Writes the Xcursor theme to <iconsDir>/theDesk-M3 (replacing it whole)
    bool writeTheme(const QString& iconsDir, const Colours& colours, const QList<int>& sizes = {24, 32, 48});
    // ~/.local/share/icons: libXcursor looks here before /usr/share/icons
    QString userIconsDir();
} // namespace M3Cursor

#endif // M3CURSOR_H
