include_guard()

function(cntp_find_tool var name)
    set(CNTP_TOOL_PATH "" CACHE PATH "Path to libcontemporary tools that can be run on the host system")

    if(CNTP_TOOL_PATH STREQUAL "")
        # ../../../../bin/
        set(BIN_HINT ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
        cmake_path(GET BIN_HINT PARENT_PATH BIN_HINT)
        cmake_path(GET BIN_HINT PARENT_PATH BIN_HINT)
        cmake_path(GET BIN_HINT PARENT_PATH BIN_HINT)
        cmake_path(GET BIN_HINT PARENT_PATH BIN_HINT)

        find_program(${var} ${name} REQUIRED
            HINTS ${BIN_HINT}/bin
        )
    else()
        find_program(${var} ${name} REQUIRED
            PATHS ${CNTP_TOOL_PATH}
            PATH_SUFFIXES bin
            NO_DEFAULT_PATH
            NO_CMAKE_PATH
        )
    endif()

    set(${var} ${${var}} PARENT_SCOPE)
endfunction()
