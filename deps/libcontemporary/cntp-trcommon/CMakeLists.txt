cmake_minimum_required(VERSION 3.21.0)

project(tltrdesktop VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

set(SOURCES
    jsonfile.cpp
)

set(HEADERS
    jsonfile.h
)

add_library(cntp-trcommon STATIC ${SOURCES} ${HEADERS})
set_target_properties(cntp-trcommon PROPERTIES
        MACOSX_BUNDLE FALSE)
cntp_asan(cntp-trcommon)

target_link_libraries(cntp-trcommon Qt::Core)

