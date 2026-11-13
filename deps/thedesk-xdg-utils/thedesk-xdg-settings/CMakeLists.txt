cmake_minimum_required(VERSION 3.21.0)

project(thedesk-xdg-settings VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(libcontemporary REQUIRED)
find_package(libtdesktopenvironment REQUIRED)

set(SOURCES
    main.cpp
)

set(HEADERS

)

add_executable(thedesk-xdg-settings ${SOURCES} ${HEADERS})

set_target_properties(thedesk-xdg-settings PROPERTIES
    CNTP_DATA_SUBDIR thedesk/xdg-utils/thedesk-xdg-settings
)

cntp_init(thedesk-xdg-settings 20)
cntp_translate(thedesk-xdg-settings)
cntp_target_name(thedesk-xdg-settings "thedesk-xdg-settings")

target_link_libraries(thedesk-xdg-settings Qt::Widgets libcontemporary libtdesktopenvironment)

install(TARGETS thedesk-xdg-settings
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

link_xdg_util(thedesk-xdg-settings xdg-settings)
