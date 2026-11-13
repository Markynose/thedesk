include_guard()

function(cntp_mac_icon targetName)
    set(singleValueArgs ICON BLUEPRINT_ICON)
    cmake_parse_arguments(MAC_ICON "" "${singleValueArgs}" "" ${ARGN})

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        if(BLUEPRINT AND DEFINED MAC_ICON_BLUEPRINT_ICON)
            set(CURRENT_ICON ${MAC_ICON_BLUEPRINT_ICON})
        else()
            set(CURRENT_ICON ${MAC_ICON_ICON})
        endif()

        IF(CURRENT_ICON)
            set_target_properties(${targetName} PROPERTIES
                    MACOSX_BUNDLE_ICON_FILE ${CURRENT_ICON})
            target_sources(${targetName} PRIVATE ${CURRENT_ICON})
            set_source_files_properties(${CURRENT_ICON} PROPERTIES
                    MACOSX_PACKAGE_LOCATION Resources)
        ELSE()
            message(WARNING "No Apple icon set for current build configuration")
        ENDIF()
    ENDIF()
endfunction()
