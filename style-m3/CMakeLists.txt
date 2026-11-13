# theDesk's Material 3 Expressive Qt widget style ("Material3"). Loaded by
# every Qt app in the session through Platform/style.
cmake_minimum_required(VERSION 3.24.0)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(libcontemporary REQUIRED)

cntp_define_build_dirs()

add_library(thedesk-style-m3 MODULE
    m3animator.cpp m3animator.h
    m3style.cpp m3style.h
    m3styleplugin.cpp m3styleplugin.h
    plugin.json)
set_target_properties(thedesk-style-m3 PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME thedesk-material3)
target_link_libraries(thedesk-style-m3 PRIVATE thedesk-m3 Qt::Widgets libcontemporary)

install(TARGETS thedesk-style-m3
        LIBRARY DESTINATION ${CNTP_INSTALL_PLUGINS}/styles/)
