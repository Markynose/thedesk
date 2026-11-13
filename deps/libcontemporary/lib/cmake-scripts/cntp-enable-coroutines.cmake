include_guard()

function(cntp_enable_coroutines target)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # GCC
        target_compile_options(${target} PUBLIC "-fcoroutines")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        target_compile_options(${target} PUBLIC "-fcoroutines-ts")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # MSVC auto-enables coroutine support when C++20 is enabled
    else()
        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not currently supported.")
    endif()
endfunction()
