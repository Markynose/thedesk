/****************************************
 *
 *   theDesk - Material 3 colour settings
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3COLOURSETTINGS_H
#define M3COLOURSETTINGS_H

#include <QWidget>

class QGridLayout;
class tSettings;
struct M3ColourSettingsPrivate;

/**
 * The Theme pane's colour settings when Palette/base=m3: a row of colour
 * swatches in the manner of Android's wallpaper colour picker (each a circle
 * split into its scheme's primary, secondary and tertiary; the first follows
 * the living horizon), then style, mode and contrast. Writes Palette/m3.* in
 * theDesk.platform; the platform theme recolours live.
 */
class M3ColourSettings : public QObject {
        Q_OBJECT
    public:
        // Adds its rows to `grid` from `row` on, reading/writing `settings`
        M3ColourSettings(QGridLayout* grid, int row, tSettings* settings, QWidget* parent);
        ~M3ColourSettings() override;

    private:
        M3ColourSettingsPrivate* d;
        void load();
};

#endif // M3COLOURSETTINGS_H
