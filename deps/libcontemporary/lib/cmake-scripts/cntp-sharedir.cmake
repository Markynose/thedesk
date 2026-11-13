include_guard()

function(cntp_get_target_share_dir_name destination target)
    get_target_property(SHARE_DIR ${target} CNTP_DATA_SUBDIR)

    if(SHARE_DIR STREQUAL "SHARE_DIR-NOTFOUND")
        message(FATAL_ERROR "Please invoke cntp_init on the target ${target}")
    endif()

    set(${destination} ${SHARE_DIR} PARENT_SCOPE)
endfunction()

function(cntp_get_target_share_subdir_name destination target)
    get_target_property(SHARE_DIR ${target} CNTP_DATA_SUBDIR_WITHOUT_PARENT)

    if(SHARE_DIR STREQUAL "SHARE_DIR-NOTFOUND")
        set(${destination} "." PARENT_SCOPE)
    else()
        set(${destination} ${SHARE_DIR} PARENT_SCOPE)
    endif()

endfunction()

function(cntp_get_target_share_dir destination target)
    include(GNUInstallDirs)
    cntp_get_target_share_dir_name(SHARE_DIR ${target})

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        get_target_property(PARENT_TARGET ${target} CNTP_PARENT_TARGET)
        IF(NOT ${PARENT_TARGET} STREQUAL "PARENT_TARGET-NOTFOUND")
            set(target ${PARENT_TARGET})
        ENDIF()
        set(${destination} $<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/${SHARE_DIR} PARENT_SCOPE)
    ELSE()
        set(${destination} ${CMAKE_INSTALL_DATADIR}/${SHARE_DIR} PARENT_SCOPE)
    ENDIF()
endfunction()