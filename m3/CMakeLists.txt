# theDesk Material 3 colour: dynamic schemes seeded from the living horizon.
# Static and position-independent, so the platform theme plugin, libthedesk
# and the widget style can each link it without another installed library.
cmake_minimum_required(VERSION 3.24.0)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
find_package(libcontemporary REQUIRED)

# androidx graphics-shapes + MaterialShapes + LoadingIndicator (m3/shapes/README.md)
include(${CMAKE_CURRENT_SOURCE_DIR}/shapes/sources.cmake)

add_library(thedesk-m3 STATIC
    ${M3_SHAPES_SOURCES}
    horizonclock.cpp horizonclock.h
    m3cursor.cpp m3cursor.h
    m3motion.cpp m3motion.h
    m3scheme.cpp m3scheme.h)
# Hidden: each library or plugin that links this keeps its own private copy
set_target_properties(thedesk-m3 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(thedesk-m3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(thedesk-m3 PUBLIC Qt::Gui Qt::Widgets libcontemporary PRIVATE material-color-utilities)
