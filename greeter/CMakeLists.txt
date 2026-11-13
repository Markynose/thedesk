# thedesk-greeter: theDesk's LightDM greeter, on the same M3LockSurface as
# the lock screen. Picks up from Plymouth on the first start after boot and
# hands over to theDesk's loading screen (see M3LockSurface).
cmake_minimum_required(VERSION 3.24.0)
include(GNUInstallDirs)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIGHTDM IMPORTED_TARGET liblightdm-gobject-1 gobject-2.0 glib-2.0)
if(NOT LIGHTDM_FOUND)
    message(STATUS "liblightdm-gobject not found: not building thedesk-greeter")
    return()
endif()

add_executable(thedesk-greeter main.cpp lightdmclient.cpp lightdmclient.h)
set_target_properties(thedesk-greeter PROPERTIES AUTOMOC ON)
target_include_directories(thedesk-greeter PRIVATE ${CMAKE_SOURCE_DIR}/libthedesk)
target_link_libraries(thedesk-greeter Qt::Widgets libcontemporary libthedesk thedesk-m3 PkgConfig::LIGHTDM)
install(TARGETS thedesk-greeter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES thedesk-greeter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/xgreeters)
