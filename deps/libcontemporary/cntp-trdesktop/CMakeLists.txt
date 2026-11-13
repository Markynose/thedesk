cmake_minimum_required(VERSION 3.21.0)

project(tltrdesktop VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Network Multimedia)

add_compile_definitions(SYSTEM_LIBRARY_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

set(SOURCES
        desktopfile.cpp
        main.cpp)

set(HEADERS
        "desktopfile.h"
    )

add_executable(cntp-trdesktop ${SOURCES} ${HEADERS})
set_target_properties(cntp-trdesktop PROPERTIES
        MACOSX_BUNDLE FALSE)
cntp_asan(cntp-trdesktop)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(cntp-trdesktop PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH True)
ENDIF()

target_link_libraries(cntp-trdesktop Qt::Widgets Qt::Svg Qt::Network Qt::Multimedia libcontemporary cntp-trcommon)
target_include_directories(cntp-trdesktop PRIVATE ../cntp-trcommon)

install(TARGETS cntp-trdesktop
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
