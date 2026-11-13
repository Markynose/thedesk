project(locker VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus Sql PrintSupport)
find_package(libcontemporary REQUIRED)
find_package(libtdesktopenvironment REQUIRED)
cntp_find_pkgconfig(X11 x11)

set(SOURCES
        main.cpp
        mainwindow.cpp
        claudelockchip.cpp
        underlineanimation.cpp
        lockmanager.cpp
    )

set(HEADERS
        mainwindow.h
        claudelockchip.h
        underlineanimation.h
        lockmanager.h
    )

add_executable(locker-ui ${SOURCES} ${HEADERS})

set_target_properties(locker-ui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER com.vicr123.locker
        WIN32_EXECUTABLE TRUE
        CNTP_DATA_SUBDIR locker)

cntp_init(locker-ui 20)
cntp_translate(locker-ui)
cntp_target_name(locker-ui "td-locker")

target_link_libraries(locker-ui Qt::Widgets Qt::DBus Qt::Sql Qt::PrintSupport libcontemporary libtdesktopenvironment libthedesk locker-common)
target_include_directories(locker-ui PUBLIC ../locker-common/)
target_include_directories(locker-ui PRIVATE ../../libthedesk/)
target_compile_definitions(locker-ui PRIVATE TD_LOCKER_CHECKER_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/td-locker-checker")

install(TARGETS locker-ui
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
        BUNDLE DESTINATION /Applications)
