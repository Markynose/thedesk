cmake_minimum_required(VERSION 3.21.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED)

add_compile_definitions(SYSTEM_LIBRARY_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

set(SOURCES
        projecttemplate.cpp
        main.cpp
)

set(HEADERS
    projecttemplate.h
)

add_executable(cntp-skel ${SOURCES} ${HEADERS})
set_target_properties(cntp-skel PROPERTIES
        MACOSX_BUNDLE FALSE)
cntp_asan(cntp-skel)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(cntp-skel PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH True)
ENDIF()

target_link_libraries(cntp-skel libcontemporary)

install(TARGETS cntp-skel
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY skels
        DESTINATION ${CMAKE_INSTALL_DATADIR}/libcontemporary)
target_compile_definitions(cntp-skel PRIVATE SKELS_PATH="${CMAKE_INSTALL_FULL_DATADIR}/libcontemporary/skels")
