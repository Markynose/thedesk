cmake_minimum_required(VERSION 3.21.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

set(SOURCES
        main.cpp
        applicationbundle.cpp
        common.cpp
        library.cpp)

set(HEADERS
        "applicationbundle.h"
        "common.h"
        "library.h")

add_executable(cntp-macdeploy ${SOURCES} ${HEADERS})
set_target_properties(cntp-macdeploy PROPERTIES
        MACOSX_BUNDLE FALSE)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(cntp-macdeploy PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH True)
ENDIF()

target_link_libraries(cntp-macdeploy libcontemporary Qt::Network)

install(TARGETS cntp-macdeploy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
