include_guard()

function(cntp_define_build_dirs)
    set(QMAKE_EXTRA_ARGS)
    IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        find_path(QMAKE_PATH
                NAMES qmake6.exe
                HINTS ${QT_HOST_PATH};${CMAKE_PREFIX_PATH}
                PATH_SUFFIXES bin)
        find_path(QT_CONF
                NAMES target_qt.conf
                HINTS ${CMAKE_PREFIX_PATH}
                PATH_SUFFIXES bin)
        IF(NOT ${QT_CONF} STREQUAL "QT_CONF-NOTFOUND")
            message("Using Qt configuration file ${QT_CONF}/target_qt.conf")
            set(QMAKE_EXTRA_ARGS -qtconf "${QT_CONF}/target_qt.conf")
        ENDIF()
    ELSE()
        find_path(QMAKE_PATH qmake6)
    ENDIF()

    IF(${QMAKE_PATH} STREQUAL "QMAKE_PATH-NOTFOUND")
        message(FATAL_ERROR "Couldn't call qmake. Ensure Qt 6 is installed correctly and qmake6 is located in your PATH.")
    ENDIF()

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(QMAKE_PATH ${QMAKE_PATH}/qmake6.exe)
    ELSE()
        set(QMAKE_PATH ${QMAKE_PATH}/qmake6)
    ENDIF()

    execute_process(
        COMMAND ${QMAKE_PATH} ${QMAKE_EXTRA_ARGS} -query QT_INSTALL_PREFIX
        RESULT_VARIABLE QT_PREFIX_RESULT
        OUTPUT_VARIABLE QT_PREFIX_DIR
    )

    string(STRIP "${QT_PREFIX_DIR}" QT_PREFIX_DIR)
    file(TO_CMAKE_PATH "${QT_PREFIX_DIR}" QT_PREFIX_DIR)

    execute_process(
        COMMAND ${QMAKE_PATH} ${QMAKE_EXTRA_ARGS} -query QT_INSTALL_PLUGINS
        RESULT_VARIABLE PLUGIN_INSTALLATION_DIR_RESULT
        OUTPUT_VARIABLE PLUGIN_INSTALLATION_DIR
    )

    string(STRIP "${PLUGIN_INSTALLATION_DIR}" PLUGIN_INSTALLATION_DIR)
    file(TO_CMAKE_PATH "${PLUGIN_INSTALLATION_DIR}" PLUGIN_INSTALLATION_DIR)

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        set(CNTP_INSTALL_PLUGINS ${PLUGIN_INSTALLATION_DIR} PARENT_SCOPE)
    ELSE()
        file(RELATIVE_PATH PLUGIN_INSTALLATION_DIR "${QT_PREFIX_DIR}" "${PLUGIN_INSTALLATION_DIR}")
        set(CNTP_INSTALL_PLUGINS ${CMAKE_INSTALL_PREFIX}/${PLUGIN_INSTALLATION_DIR} PARENT_SCOPE)
    ENDIF()
endfunction()
