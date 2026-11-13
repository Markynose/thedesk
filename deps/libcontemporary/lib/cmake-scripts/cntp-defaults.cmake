include_guard()

function(cntp_defaults_file targetName)
    set(multiValueArgs DEFAULTS_FILE)
    cmake_parse_arguments(DEFAULTS "" "" "${multiValueArgs}" ${ARGN})

    foreach(DEFAULTS_FILE ${DEFAULTS_DEFAULTS_FILE})
        IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            target_sources(${targetName} PRIVATE ${DEFAULTS_FILE})
            set_source_files_properties(${DEFAULTS_FILE} PROPERTIES
                    MACOSX_PACKAGE_LOCATION Resources/defaults)
        ENDIF()

        IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            install(FILES ${DEFAULTS_FILE}
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/${SHARE_DIR}/defaults)
        ENDIF()
    endforeach()
endfunction()
