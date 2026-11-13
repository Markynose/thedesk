# Vendored dependencies

theDesk's own libraries (and, in `../apps/`, theSuite apps), copied into this repo so they can be patched alongside
it (several had bugs or Qt 6.12 issues). Build and install everything, in
order, with `scripts/build.sh`.

| Directory | Upstream | Commit (date) |
|---|---|---|
| `libcontemporary/` | https://github.com/theCheeseboard/libcontemporary | `5918edfd3a6c` (2025-07-11) |
| `libtdesktopenvironment/` | https://github.com/theCheeseboard/libtdesktopenvironment | `7c6422ae1303` (2023-11-28) |
| `libthefrisbee/` | `libthefrisbee/` from https://github.com/theCheeseboard/thefrisbee | `d3b3907a06f6` (2024-01-16) |
| `thefile/` | https://github.com/theCheeseboard/thefile (libthefile + theFile app) | `2ca6ec394ac4` (2024-01-09) |
| `contemporary-theme/` | https://github.com/theCheeseboard/contemporary-theme (Qt style) | `d4a9d63966d5` (2023-03-11) |
| `../apps/thecalculator/` | https://github.com/theCheeseboard/thecalculator | `1a3fa3fbfa96` (2023-06-09) |
| `../apps/theheartbeat/` | https://github.com/theCheeseboard/theHeartbeat | `e577d0132ea3` (2023-06-09) |
| `material-color-utilities/` | `cpp/` from https://github.com/material-foundation/material-color-utilities (Apache 2.0) | `194144d75ed7` (2026-11-06) |
| `thedesk-xdg-utils/` | https://github.com/theCheeseboard/thedesk-xdg-utils | `462e671ed58f` (2023-06-07) |

`libtdesktopenvironment` includes its git submodules (`wayland-protocols`,
`wlr-protocols`) as plain files.

These replace the AUR packages of the same names; don't install both.

## Local changes

Every change made here on top of upstream is listed below.

- `libtdesktopenvironment/plugins/x11/Wm/x11backend.cpp`: grab global keys
  with every CapsLock/NumLock combination, so shortcuts keep working while a
  lock key is on.
- `libtdesktopenvironment/plugins/x11/Wm/x11xsettingsprovider.{h,cpp}`:
  XSETTINGS carries the session's real pointer theme and size (Xcursor
  resources, else `XCURSOR_THEME`/`XCURSOR_SIZE`) and the application font,
  not the hard-coded `contemporary_cursors` / `Contemporary 10`. When a theme
  in `~/.local/share/icons` or `~/.icons` changes on disk, the root window's
  pointer is reloaded and the theme announced again, so running Qt apps drop
  their cursor cache.
- `libtdesktopenvironment/plugins/x11/Screens/x11screen{,backend}.cpp`:
  `XRRGetScreenResourcesCurrent` instead of `XRRGetScreenResources`, which
  re-probed every output (~0.7 s each) on every call.
- `libtdesktopenvironment/plugins/x11/Wm/x11keyboardtables.h`: period, comma,
  minus and slash map to the main keyboard's keys before the keypad's, so
  Super+. grabs the full stop, not the keypad's decimal point.
- `libtdesktopenvironment/lib/UPower/desktopupower.{h,cpp}`: `displayDevice()`,
  UPower's combined battery, so laptops with two batteries warn once at the
  figure the bar shows.
- `libtdesktopenvironment/lib/Screens/screendaemon.{h,cpp}`:
  `isKnownArrangement()`, whether an arrangement was saved for the screens
  connected now (theDesk asks how to use a display in a new combination).
- `libtdesktopenvironment/CMakeLists.txt`: also look for `qtwaylandscanner`
  in Qt's libexec directory (`/usr/lib/qt6` on Arch/Artix), which isn't on PATH.
- `libtdesktopenvironment/CMakeLists.txt`: set `QT_FIND_PRIVATE_MODULES` so
  `Qt::GuiPrivate`/`Qt::WaylandClientPrivate` resolve on Qt >= 6.10.
- `libtdesktopenvironment`: new `TDESKTOPENVIRONMENT_BUILD_WAYLAND` option
  (default ON; `scripts/build.sh` sets it OFF). The Wayland backend and
  layer-shell code use Qt Wayland private APIs removed in Qt 6.12
  (`QWaylandWindow::handleExpose`), and this setup is X11-only.
- `libthefrisbee`, `thefile`, `thedesk-xdg-utils`, `contemporary-theme/demo`:
  `cmake_minimum_required` raised to 3.16; CMake 4 rejects minimums below 3.5.
  The installed `libtdesktopenvironmentConfig.cmake` only requires the
  layer-shell package when it was built.
- `libcontemporary/lib/cmake-scripts/cntp-blueprint.cmake`: `FORCE_STABLE`
  now also clears a `BLUEPRINT` value cached by an earlier configure.
- `libtdesktopenvironment/plugins/x11/Screens/x11screenbackend.cpp`:
  `primaryScreen()` falls back to the first screen when none has the RandR
  primary flag. It required two or more screens, so a single-screen laptop
  had no primary screen at all (Gateway crashed, bar didn't size itself).
- `libtdesktopenvironment/lib/SystemSlide`: Enter/Return/Space lift the slide
  (lock screen) like dragging it up.
- `libtdesktopenvironment/lib/UPower/desktopupower.cpp`: the battery summary
  uses UPower's DisplayDevice (all batteries combined, as in XFCE/GNOME)
  instead of the first battery plus "N more batteries".
- `libtdesktopenvironment/plugins/x11/Wm/x11backend.cpp`: `desktops()` only
  drops a trailing *empty* name (bspwm doesn't NUL-terminate the last one, so
  its last desktop showed as "Desktop 9"); fallback names are 1-based.
- `libtdesktopenvironment/lib/SystemSlide`: `setBackgroundWidget()` lets the
  lock screen use a live widget (theDesk's horizon) instead of a pixmap.
- `material-color-utilities/`: only the C++ sources (no tests). Abseil
  removed (`HexFromArgb` uses `snprintf`, `wsmeans` uses
  `std::unordered_map`); `temperature_cache.h` gains a missing
  `#include <optional>`. Built as a static library inside theDesk.
- `libcontemporary/lib/tswitch.cpp`, `tcircularspinner.cpp`: when the Qt
  style answers style hint `SH_CustomBase + 0x4D33` (theDesk's Material3
  style does), the switch and spinner ask the style to draw them
  (`PE_CustomBase + 0x4D33` / `+ 0x4D34`): an M3 switch and the M3 Expressive
  loading indicator. Other styles get the original drawing.
- `libcontemporary/lib/tpopover.cpp`: under the Material3 style, popovers
  become M3 sheets: a rounded SurfaceContainerLow backdrop (28 dp top corners
  for bottom sheets, 16 dp for side sheets), a drag handle on bottom sheets,
  and no separator line.

### libcontemporary: Material 3 labels and lines (stage 9 B1)

When the app runs theDesk's Material3 style (`libContemporaryCommon::isMaterial3()`):
`tTitleLabel` is a small top app bar (22 px TitleLarge, round 40 dp back
button, no bottom rule); `tSubtitleLabel` is TitleSmall in Primary, sentence
case; `lineColor()` is the text colour at 16% (OutlineVariant-like) instead of
full strength. Other styles are unchanged.
