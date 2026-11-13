/****************************************
 *
 *   theDesk - session colours for other programs
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef THEMESYNC_H
#define THEMESYNC_H

/**
 * Carries the session's M3 scheme (the living horizon's, or the chosen
 * colour) to programs that don't read theDesk's palette:
 *
 *   kitty   ~/.config/kitty/thedesk-m3.conf (kitty.conf includes it last);
 *           running kittys reload it (SIGUSR1)
 *   bspwm   window borders: focused Primary, others the outline tones
 *   GTK 3   the theme theDesk-M3 (~/.local/share/themes): adw-gtk3-dark with
 *           its colours rewritten; Firefox's system theme follows it
 *   GTK 4   ~/.config/gtk-4.0/thedesk-m3.css, imported by gtk.css
 *           (libadwaita's named colours); both from each app's next start
 *   greeter the colour choice and the user's picture, in LightDM's per-user
 *           greeter data folder
 *
 * startdesk calls apply() at login and whenever the palette shifts.
 * Only when theDesk's palette is M3 (Palette/base=m3).
 */
namespace ThemeSync {
    bool enabled();
    void apply();
    void writeGreeterPalette(); // for the login screen ($XDG_GREETER_DATA_DIR)
    void writeGreeterPicture(); // ~/.face, which a private home hides from it
} // namespace ThemeSync

#endif // THEMESYNC_H
