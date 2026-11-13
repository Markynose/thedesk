# The theDesk-M3 cursor theme (m3/m3cursor.h): generated at build time in
# neutral colours and installed to share/icons; startdesk rewrites a copy in
# the session's M3 colours under ~/.local/share/icons at login.
cmake_minimum_required(VERSION 3.24.0)
include(GNUInstallDirs)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)
add_executable(m3-cursors main.cpp)
target_link_libraries(m3-cursors thedesk-m3 Qt::Gui)

set(M3_CURSOR_DIR ${CMAKE_CURRENT_BINARY_DIR}/icons)
add_custom_command(OUTPUT ${M3_CURSOR_DIR}/theDesk-M3/index.theme
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen $<TARGET_FILE:m3-cursors> ${M3_CURSOR_DIR}
    DEPENDS m3-cursors
    COMMENT "Rendering the theDesk-M3 cursor theme")
add_custom_target(m3-cursor-theme ALL DEPENDS ${M3_CURSOR_DIR}/theDesk-M3/index.theme)
install(DIRECTORY ${M3_CURSOR_DIR}/theDesk-M3 DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
