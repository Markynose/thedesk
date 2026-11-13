cmake_minimum_required(VERSION 3.24.0)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(libcontemporary)

set(SOURCES
    onboarding/onboardingtheme.cpp
    plugin.cpp
    settings/accentcolourpicker.cpp
    settings/m3coloursettings.cpp
    settings/splashsettings.cpp
    settings/themesettingspane.cpp

    onboarding/onboardingtheme.ui
    settings/themesettingspane.ui

    thedesk-themeplugin-resources.qrc
)

set(HEADERS
    onboarding/onboardingtheme.h
    plugin.h
    settings/accentcolourpicker.h
    settings/m3coloursettings.h
    settings/splashsettings.h
    settings/themesettingspane.h
)

set(PLUGIN_NAME theme)
add_plugin_td(${PLUGIN_NAME})
target_sources(plugin-${PLUGIN_NAME} PRIVATE ${SOURCES} ${HEADERS})
cntp_defaults_file(${PLUGIN_NAME}
    DEFAULTS_FILE thedesk-theme.conf)

# The loading screen's own view, for the live preview
target_include_directories(plugin-${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libthedesk)
target_link_libraries(plugin-${PLUGIN_NAME} Qt::Widgets libcontemporary libthedesk thedesk-m3)
