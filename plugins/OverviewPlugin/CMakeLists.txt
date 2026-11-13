cmake_minimum_required(VERSION 3.24.0)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus Svg)
find_package(libcontemporary)

set(SOURCES
    OverviewPane/overviewpane.cpp
    OverviewPane/worldclock.cpp
    OverviewPane/m3overview.cpp
    clockchunk.cpp
    claude/claudechunk.cpp
    plugin.cpp

    OverviewPane/overviewpane.ui
    OverviewPane/worldclock.ui
    clockchunk.ui
)

set(HEADERS
    OverviewPane/overviewpane.h
    OverviewPane/worldclock.h
    OverviewPane/m3overview.h
    clockchunk.h
    claude/claudechunk.h
    plugin.h
)

set(PLUGIN_NAME overview)
add_plugin_td(${PLUGIN_NAME})
target_sources(plugin-${PLUGIN_NAME} PRIVATE ${SOURCES} ${HEADERS})

target_link_libraries(plugin-${PLUGIN_NAME} Qt::Widgets Qt::Svg Qt::Multimedia libcontemporary libthedesk)
