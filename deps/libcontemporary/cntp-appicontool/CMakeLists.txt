cmake_minimum_required(VERSION 3.21.0)

project(tltrdesktop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Svg Xml)

add_compile_definitions(SYSTEM_LIBRARY_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

set(SOURCES
        combinedicon.cpp
        platformicongenerator.cpp
        main.cpp

        resources.qrc)

set(HEADERS
    "combinedicon.h"
    "platformicongenerator.h")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(SOURCES ${SOURCES} platformgenerators/macplatformicongenerator.cpp)
    set(HEADERS ${HEADERS} platformgenerators/macplatformicongenerator.h)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(SOURCES ${SOURCES} platformgenerators/winplatformicongenerator.cpp)
    set(HEADERS ${HEADERS} platformgenerators/winplatformicongenerator.h)
ENDIF()

add_executable(cntp-appicontool ${SOURCES} ${HEADERS})
set_target_properties(cntp-appicontool PROPERTIES
        MACOSX_BUNDLE FALSE)
cntp_asan(cntp-appicontool)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(cntp-appicontool PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH True)
ENDIF()

target_link_libraries(cntp-appicontool Qt::Svg Qt::Xml libcontemporary)

install(TARGETS cntp-appicontool
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
