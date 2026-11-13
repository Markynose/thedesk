include_guard()

function(cntp_build_translations target)
    message("-- Finding translation files to build translations for (target ${target})")

    file(GLOB TRANSLATION_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts)
    list(PREPEND TRANSLATION_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/en_US.ts)

    add_custom_target(${target}_CNTPBUILDTRANSLATIONS
            COMMAND lrelease ${TRANSLATION_FILES} -silent
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Building translation files"
            VERBATIM)
    add_dependencies(${target}_CNTPBUILDTRANSLATIONS ${target}_CNTPTRANSLATIONS)
    add_dependencies(${target} ${target}_CNTPBUILDTRANSLATIONS)
endfunction()
