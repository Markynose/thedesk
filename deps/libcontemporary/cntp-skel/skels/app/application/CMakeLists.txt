cmake_minimum_required(VERSION 3.21.0)

project($($LCPNAME) VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(libcontemporary REQUIRED)

set(SOURCES
        main.cpp
        mainwindow.cpp mainwindow.ui
)

set(HEADERS
        mainwindow.h
)

# Find Apple lproj files
#set(LPROJ_SOURCES)
#file(GLOB LPROJ_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/translations/apple-lproj/*.lproj)
#FOREACH(DIR ${LPROJ_DIRS})
#    cmake_path(GET DIR FILENAME DIR_NAME)
#    file(GLOB LPROJ_FILES ${DIR}/*.strings)
#    FOREACH(FILE ${LPROJ_FILES})
#        list(APPEND LPROJ_SOURCES ${FILE})
#        set_source_files_properties(${FILE} PROPERTIES
#                MACOSX_PACKAGE_LOCATION Resources/${DIR_NAME})
#    ENDFOREACH()
#ENDFOREACH()
#set(SOURCES ${SOURCES} ${LPROJ_SOURCES})

add_executable($($LCPNAME) ${SOURCES} ${HEADERS})

set_target_properties($($LCPNAME) PROPERTIES
        MACOSX_BUNDLE TRUE
#        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER $($desktop-id)
        WIN32_EXECUTABLE TRUE
        CNTP_DATA_SUBDIR $($LCPNAME))

cntp_init($($LCPNAME) 20)
cntp_translate($($LCPNAME))
cntp_target_name($($LCPNAME) "$($PNAME)")
cntp_app_icon($($LCPNAME)
    BASEICON icons/baseicon.svg
    BASEICON_MAC icons/baseicon-mac.svg)
cntp_target_desktop_file($($LCPNAME)
    DESKTOP_FILE $($desktop-id).desktop
    BLUEPRINT_DESKTOP_FILE $($desktop-id)_blueprint.desktop)
cntp_defaults_file($($LCPNAME)
    DEFAULTS_FILE $($LCPNAME).conf)

target_link_libraries($($LCPNAME) Qt::Widgets libcontemporary lib$($LCPNAME))
target_include_directories($($LCPNAME) PUBLIC ../lib$($LCPNAME)/)

install(TARGETS $($LCPNAME)
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION /Applications)
