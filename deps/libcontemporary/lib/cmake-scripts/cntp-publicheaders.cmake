include_guard()

function(cntp_target_public_headers target)
    include(GNUInstallDirs)

    set(singleValueArgs DESTINATION_DIRECTORY)
    set(multiValueArgs HEADERS)
    cmake_parse_arguments(INSTALL_HEADERS "" "${singleValueArgs}"
            "${multiValueArgs}" ${ARGN} )

    IF(NOT DEFINED INSTALL_HEADERS_DESTINATION_DIRECTORY)
        message(FATAL_ERROR "DESTINATION_DIRECTORY not defined for public headers")
    ENDIF()
    IF(NOT DEFINED INSTALL_HEADERS_HEADERS)
        message(FATAL_ERROR "HEADERS not defined for public headers")
    ENDIF()

    foreach(file IN LISTS INSTALL_HEADERS_HEADERS)
        cmake_path(ABSOLUTE_PATH file)
        cmake_path(GET file PARENT_PATH fileParent)
        cmake_path(RELATIVE_PATH fileParent OUTPUT_VARIABLE relfile)
        target_sources(${target} PRIVATE ${file})

        IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            set_source_files_properties(${file} PROPERTIES
                    MACOSX_PACKAGE_LOCATION Headers/${relfile})
        ELSE()
            install(FILES ${file}
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${INSTALL_HEADERS_DESTINATION_DIRECTORY}/${relfile})
        ENDIF()
    ENDFOREACH()

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        get_target_property(TARGET_OUTPUT_NAME ${target} OUTPUT_NAME)

        IF(TARGET_OUTPUT_NAME STREQUAL "TARGET_OUTPUT_NAME-NOTFOUND")
            set(TARGET_OUTPUT_NAME ${target})
        ENDIF()

        set(${target}_INCLUDE_DIR ${CMAKE_INSTALL_LIBDIR}/${TARGET_OUTPUT_NAME}.framework/Headers PARENT_SCOPE)
    ELSE()
        set(${target}_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${INSTALL_HEADERS_DESTINATION_DIRECTORY} PARENT_SCOPE)
    ENDIF()
endfunction()
