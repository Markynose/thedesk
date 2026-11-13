# thedesk-boot-theme: renders the boot path's art from the horizon scene,
# shapes and M3 scheme: Limine's menu (scripts/install-limine.sh runs it for
# the screen's size) and the Plymouth theme "thedesk" (rendered at build time
# and installed to share/plymouth/themes/thedesk).
cmake_minimum_required(VERSION 3.24.0)
include(GNUInstallDirs)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)
add_executable(thedesk-boot-theme main.cpp boot-theme.qrc)
set_target_properties(thedesk-boot-theme PROPERTIES AUTORCC ON)
target_include_directories(thedesk-boot-theme PRIVATE ${CMAKE_SOURCE_DIR}/libthedesk)
target_link_libraries(thedesk-boot-theme libthedesk thedesk-m3 Qt::Gui)
install(TARGETS thedesk-boot-theme RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(PLYMOUTH_THEME_DIR ${CMAKE_CURRENT_BINARY_DIR}/plymouth/thedesk)
add_custom_command(OUTPUT ${PLYMOUTH_THEME_DIR}/thedesk.plymouth
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
        LD_LIBRARY_PATH=$<TARGET_FILE_DIR:libthedesk>
        $<TARGET_FILE:thedesk-boot-theme> plymouth ${PLYMOUTH_THEME_DIR}
    DEPENDS thedesk-boot-theme thedesk.script
    COMMENT "Rendering the thedesk Plymouth theme")
add_custom_target(plymouth-theme ALL DEPENDS ${PLYMOUTH_THEME_DIR}/thedesk.plymouth)
install(DIRECTORY ${PLYMOUTH_THEME_DIR} DESTINATION ${CMAKE_INSTALL_DATADIR}/plymouth/themes)
