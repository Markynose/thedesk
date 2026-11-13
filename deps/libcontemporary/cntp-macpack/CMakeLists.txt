cmake_minimum_required(VERSION 3.21.0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Svg)

set(SOURCES
    main.mm
    diskimage.cpp
    dsstore.cpp
    dsstoreentry.cpp
    utils.cpp
    alias.cpp

    resources.qrc
)

set(HEADERS
    diskimage.h
    dsstore.h
    dsstoreentry.h
    utils.h
    alias.h
)

add_executable(cntp-macpack ${SOURCES} ${HEADERS})
set_target_properties(cntp-macpack PROPERTIES
        MACOSX_BUNDLE FALSE)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(cntp-macpack PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH True)
ENDIF()

target_link_libraries(cntp-macpack libcontemporary Qt::Core Qt::Svg)

install(TARGETS cntp-macpack
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
