include_guard()

function(cntp_target_install_appstream_metainfo_file target appstreamfile)
    IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        message("-- Finding appstream metainfo files to install for (target ${target})")
        get_filename_component(appstreamfile_noext ${appstreamfile} NAME_WLE)

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${appstreamfile_noext}.xml
                DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo/)
    ENDIF()
endfunction()

function(cntp_target_translate_appstream_metainfo_file target appstreamfile)
    cntp_find_tool(CNTPTRAPPSTREAM_PATH cntp-trappstream)
    message("-- Finding appstream metainfo files to generate translations for (target ${target})")
    get_filename_component(appstreamfile_noext ${appstreamfile} NAME_WLE)

    add_custom_target(${target}_CNTPAPPSTREAMTRANSLATIONS_${appstreamfile_noext}
            COMMAND ${CNTPTRAPPSTREAM_PATH} --json-generate --json-directory ${CMAKE_CURRENT_SOURCE_DIR}/translations/appstream/${appstreamfile_noext}/ --metainfo-template ${appstreamfile}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating appstream metainfo translation files"
            VERBATIM)
    add_dependencies(${target} ${target}_CNTPAPPSTREAMTRANSLATIONS_${appstreamfile_noext})

    add_custom_target(${target}_CNTPAPPSTREAMTRANSLATIONMERGE_${appstreamfile_noext}
            COMMAND ${CNTPTRAPPSTREAM_PATH} --metainfo-generate --json-directory ${CMAKE_CURRENT_SOURCE_DIR}/translations/appstream/${appstreamfile_noext}/ --metainfo-template ${appstreamfile} --metainfo-output ${CMAKE_CURRENT_BINARY_DIR}/${appstreamfile_noext}.xml
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating appstream metainfo file from translations"
            VERBATIM)
    add_dependencies(${target} ${target}_CNTPAPPSTREAMTRANSLATIONMERGE_${appstreamfile_noext})
endfunction()

function(cntp_target_appstream_metainfo_file target)
    set(multiValueArgs METAINFO_FILE BLUEPRINT_METAINFO_FILE)
    cmake_parse_arguments(METAINFO_FILE "" "" "${multiValueArgs}" ${ARGN})

    foreach(METAINFO_FILE ${METAINFO_FILE_BLUEPRINT_METAINFO_FILE} ${METAINFO_FILE_METAINFO_FILE})
        cntp_target_translate_appstream_metainfo_file(${target} ${METAINFO_FILE})
    endforeach()

    if(BLUEPRINT AND DEFINED METAINFO_FILE_BLUEPRINT_METAINFO_FILE)
        foreach(METAINFO_FILE ${METAINFO_FILE_BLUEPRINT_METAINFO_FILE})
            cntp_target_install_appstream_metainfo_file(${target} ${METAINFO_FILE})
        endforeach()
    else()
        foreach(METAINFO_FILE ${METAINFO_FILE_METAINFO_FILE})
            cntp_target_install_appstream_metainfo_file(${target} ${METAINFO_FILE})
        endforeach()
    endif()
endfunction()
