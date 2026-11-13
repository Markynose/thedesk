include_guard()

function(cntp_dmg target svg)
    get_target_property(INSTALL_TARGET ${target} CNTP_PARENT_TARGET)
    if(NOT INSTALL_TARGET STREQUAL "INSTALL_TARGET-NOTFOUND")
        message(FATAL_ERROR "Cannot write dmg metadata to a plugin")
    endif()

    target_sources(${target} PRIVATE ${svg})
    set_source_files_properties(${svg} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources/cntp/dmg)
endfunction()
