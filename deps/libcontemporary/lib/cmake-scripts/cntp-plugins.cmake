include_guard()

function(cntp_plugin_directory target plugin_name)
    include(GNUInstallDirs)
    cntp_get_target_share_dir_name(SHARE_DIR ${target})

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(${plugin_name}_INSTALL_LIB $<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/Plugins PARENT_SCOPE)
    ELSE()
        set(${plugin_name}_INSTALL_LIB ${CMAKE_INSTALL_LIBDIR}/${SHARE_DIR}/plugins PARENT_SCOPE)
    ENDIF()
endfunction()

function(cntp_install_plugin target plugin_name)
    cntp_plugin_directory(${target} ${plugin_name})

    install(TARGETS ${plugin_name}
            LIBRARY DESTINATION ${${plugin_name}_INSTALL_LIB})
endfunction()