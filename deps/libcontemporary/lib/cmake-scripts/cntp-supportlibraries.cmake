include_guard()

function(cntp_mark_support_library_directory)
    file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/.cntp-is-support-library)
endfunction()
