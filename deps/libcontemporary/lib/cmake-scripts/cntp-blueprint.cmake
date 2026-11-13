include_guard(GLOBAL)

option(FORCE_BLUEPRINT "Force a Blueprint build (FORCE_STABLE overrides)" OFF)
option(FORCE_STABLE "Force a Stable build (overrides FORCE_BLUEPRINT)" OFF)

if(FORCE_STABLE)
    message(STATUS "Forcing a Stable build")
    # Clear a BLUEPRINT value cached by an earlier configure
    set(BLUEPRINT OFF CACHE INTERNAL "Whether the project is built as Blueprint or not (Use FORCE_STABLE or FORCE_BLUEPRINT to override)" FORCE)
elseif(FORCE_BLUEPRINT)
    message(STATUS "Forcing a Blueprint build")
    set(BLUEPRINT ON CACHE INTERNAL "Whether the project is built as Blueprint or not (Use FORCE_STABLE or FORCE_BLUEPRINT to override)" FORCE)
else()
    find_program(CNTP_GIT_COMMAND git)
    if(CNTP_GIT_COMMAND)
        execute_process(COMMAND ${CNTP_GIT_COMMAND} rev-parse --show-toplevel
            OUTPUT_VARIABLE GIT_ROOT_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        execute_process(COMMAND ${CNTP_GIT_COMMAND} tag --points-at HEAD
            OUTPUT_VARIABLE CNTP_GIT_OUTPUT
            RESULT_VARIABLE CNTP_GIT_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        if(CNTP_GIT_RESULT EQUAL 0)
            if("${CNTP_GIT_OUTPUT}" STREQUAL "")
                if(NOT EXISTS "${GIT_ROOT_DIR}/PKGBUILD")
                    set(BLUEPRINT ON CACHE INTERNAL "Whether the project is built as Blueprint or not (Use FORCE_STABLE or FORCE_BLUEPRINT to override)" FORCE)
                endif()
            endif()
        else()
            message("Cannot determine Blueprint status from Git; maybe not a Git repository?")
        endif()
    else()
        message("Cannot determine Blueprint status because Git is not installed")
    endif()
endif()

if(BLUEPRINT)
    message(STATUS "Building as Blueprint")
    add_compile_definitions(T_BLUEPRINT_BUILD)
else()
    message(STATUS "Building as Stable")
endif()
