/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3SETTINGSGROUP_H
#define M3SETTINGSGROUP_H

#include <functional>
#include <m3scheme.h>

class QPalette;
class QWidget;

/**
 * Android 16's settings groups for Designer forms: put m3SettingsGroup=true
 * on a container with a QGridLayout or QVBoxLayout and its rows become
 * segmented tiles (2 dp apart, outer corners 20, inner 4), headers and
 * free text sit between them, a label right under a tile becomes its
 * supporting text. A row of a title label and one combo box shows the
 * value under the title and the combo shrinks to an unfold button whose
 * menu opens from the row; a lone push button becomes a row you click.
 * Clicking anywhere on a tile works its control.
 *
 * Opt-ins on the row's widgets: m3Tile=true/false forces a row in or out,
 * m3Icon="bedtime" (a Material Symbols name) on the title label gives the
 * row a leading symbol.
 *
 * For search, the container publishes m3Tiles (a list of maps: title,
 * control, value, section, all QObject*); setting m3Flash to a widget in a
 * tile pulses that tile twice.
 */
namespace M3SettingsGroup {
    using SchemeFor = std::function<const M3::Scheme&(const QPalette&)>;
    bool wanted(const QWidget* widget);
    void attach(QWidget* container, SchemeFor scheme);
    bool rowButton(const QWidget* widget); // a push button drawn as a row
    bool rowCombo(const QWidget* widget);  // a combo box drawn as an unfold button
} // namespace M3SettingsGroup

#endif // M3SETTINGSGROUP_H
