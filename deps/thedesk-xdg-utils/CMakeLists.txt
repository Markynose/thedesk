cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

project(thedesk-xdg-utils VERSION 1.0.0 LANGUAGES CXX)

find_package(QT REQUIRED NAMES Qt6)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    link_directories(/usr/local/lib)
ENDIF()

function(link_xdg_util binary xdgname)
    file(RELATIVE_PATH SYMLINK_PATH ${CMAKE_INSTALL_FULL_LIBDIR}/thedesk/xdg-utils ${CMAKE_INSTALL_FULL_BINDIR}/${binary})
    install(CODE
        "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_FULL_LIBDIR}/thedesk/xdg-utils)
         execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${SYMLINK_PATH} ${CMAKE_INSTALL_FULL_LIBDIR}/thedesk/xdg-utils/${xdgname})"
    )

endfunction()

add_subdirectory(thedesk-open)
add_subdirectory(thedesk-xdg-settings)

include(GNUInstallDirs)

file(RELATIVE_PATH SYMLINK_PATH ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_LIBDIR}/thedesk/xdg-utils)
install(CODE
    "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_FULL_BINDIR})
     execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${SYMLINK_PATH} ${CMAKE_INSTALL_FULL_BINDIR}/thedesk-xdg-utils)"
)
