include_guard()

function(cntp_sourcelist)
    set(oneValueArgs TARGET_SYSTEM_NAME OUTPUT OUTPUT_ALL)
    set(multiValueArgs FILES)
    cmake_parse_arguments(SOURCELIST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    list(APPEND ${SOURCELIST_OUTPUT_ALL} ${SOURCELIST_FILES})

    if("${SOURCELIST_TARGET_SYSTEM_NAME}" STREQUAL "")
        list(APPEND ${SOURCELIST_OUTPUT} ${SOURCELIST_FILES})
    elseif("${SOURCELIST_TARGET_SYSTEM_NAME}" MATCHES ${CMAKE_SYSTEM_NAME})
        list(APPEND ${SOURCELIST_OUTPUT} ${SOURCELIST_FILES})
    endif()

    set(${SOURCELIST_OUTPUT_ALL} ${${SOURCELIST_OUTPUT_ALL}} PARENT_SCOPE)
    set(${SOURCELIST_OUTPUT} ${${SOURCELIST_OUTPUT}} PARENT_SCOPE)
endfunction()
