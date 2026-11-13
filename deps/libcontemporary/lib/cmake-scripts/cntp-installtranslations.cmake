include_guard()

function(cntp_install_translations target)
    include(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cntp-sharedir.cmake)
    cntp_get_target_share_dir(SHARE_DIR ${target})

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/translations
            DESTINATION ${SHARE_DIR}
            FILES_MATCHING PATTERN *.qm)
endfunction()
