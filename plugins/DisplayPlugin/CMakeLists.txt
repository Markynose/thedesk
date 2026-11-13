cmake_minimum_required(VERSION 3.24.0)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus Positioning)
find_package(libcontemporary)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XRANDR REQUIRED IMPORTED_TARGET xrandr x11)

set(SOURCES
    arrange/arrangecontroller.cpp
    arrange/arrangewidget.cpp
    arrange/overlaywindow.cpp
    arrange/screenarranger.cpp
    newdisplay/displaychooser.cpp
    newdisplay/displayhandoff.cpp
    newdisplay/displaywaitingscreen.cpp
    newdisplay/newdisplaycontroller.cpp
    newdisplay/xrandrlayout.cpp
    plugin.cpp
    redshift/colorramp.cpp
    redshift/redshiftdaemon.cpp
    settings/displaysettings.cpp

    arrange/arrangewidget.ui
    arrange/overlaywindow.ui
    settings/displaysettings.ui
)

set(HEADERS
    arrange/arrangecontroller.h
    arrange/arrangewidget.h
    arrange/overlaywindow.h
    arrange/screenarranger.h
    newdisplay/displaychooser.h
    newdisplay/displayhandoff.h
    newdisplay/displaywaitingscreen.h
    newdisplay/newdisplaycontroller.h
    newdisplay/xrandrlayout.h
    plugin.h
    redshift/colorramp.h
    redshift/redshiftdaemon.h
    settings/displaysettings.h
)

set(PLUGIN_NAME display)
add_plugin_td(${PLUGIN_NAME})
target_sources(plugin-${PLUGIN_NAME} PRIVATE ${SOURCES} ${HEADERS})
cntp_defaults_file(${PLUGIN_NAME}
    DEFAULTS_FILE thedesk-display.conf)

target_link_libraries(plugin-${PLUGIN_NAME} Qt::Widgets Qt::Multimedia Qt::Positioning libcontemporary libthedesk thedesk-m3 PkgConfig::XRANDR)
